/* nx_splitjoin.c -- reassemble Unity's `.splitN` asset files on the SD card,
 * automatically, at boot.
 *
 * WHY
 * ---
 * Some Unity Android builds chop large files in assets/bin/Data into 1 MiB
 * chunks:
 *
 *     level8.split0   1048576
 *     level8.split1    449424     ->  level8
 *
 * On a phone Unity reassembles these through a JAVA class: the string
 * "AndroidSplitFile" sits in libunity.so immediately beside
 * "Unable to resolve method", which marks it as a JNI FindClass/GetMethodID
 * target rather than native code. This loader has no Java runtime, so nothing
 * reassembles them and Unity sees the base files as missing.
 *
 * Rather than make the user run a PC-side script, we do it here on first boot.
 * The result is byte-for-byte the file Unity would have produced, so after the
 * first run the data directory looks exactly like a desktop build's and this
 * pass becomes a no-op.
 *
 * WHY NOT A VIRTUAL FILE LAYER
 * ----------------------------
 * Intercepting open/read/lseek/fstat/mmap to present a concatenated view would
 * avoid the SD writes, but it puts a new indirection under every asset read
 * Unity performs -- including the memory-mapped ones -- for the sake of about
 * 8 MB. Joining once is far less risk for the same result.
 *
 * SAFETY
 * ------
 *  - writes to "<base>.tmp" and renames only after the whole join succeeded, so
 *    an interrupted boot can never leave a short file that LOOKS complete;
 *  - verifies the result against the Unity header's own declared file size
 *    where the file is a serialized file, and deletes it if that disagrees;
 *  - never deletes the .splitN parts (see JOIN_DELETE_PARTS in config.h);
 *  - skips any base that already exists, so it is cheap on every later boot.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "util.h"      /* debugPrintf */

#ifndef JOIN_DELETE_PARTS
#define JOIN_DELETE_PARTS 0
#endif

#define JOIN_BUF (256 * 1024)
#define JOIN_MAX_SETS 64
#define JOIN_MAX_PARTS 64

static char g_join_buf[JOIN_BUF];

static uint32_t be32(const unsigned char *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static uint64_t be64(const unsigned char *p) {
  return ((uint64_t)be32(p) << 32) | (uint64_t)be32(p + 4);
}

/* A Unity SerializedFile records its own total length, so a short or
 * misordered join is detectable. Layout calibrated against this game's
 * UNSPLIT level0 (69688 bytes, header declares 69688):
 *     @8  u32 version      (0x16 == 22 here)
 *     @24 u64 fileSize     big-endian, when version >= 22
 *     @4  u32 fileSize     big-endian, older versions
 * Files that are not serialized files (.resource blobs and the like) have no
 * such field; those are accepted on byte count alone. */
static int join_size_ok(const char *path, long long actual) {
  unsigned char h[48];
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  size_t n = fread(h, 1, sizeof h, f);
  fclose(f);
  if (n < 40) return 1;                       /* too small to carry a header */
  uint32_t ver = be32(h + 8);
  if (ver < 5 || ver > 40) return 1;          /* not a serialized file */
  long long declared = (ver >= 22) ? (long long)be64(h + 24)
                                   : (long long)be32(h + 4);
  if (declared != actual) {
    debugPrintf("[join] header says %lld bytes, wrote %lld -- REJECTED\n",
                declared, actual);
    return 0;
  }
  return 1;
}

/* concatenate <dir>/<base>.split0.. into <dir>/<base>; returns 0 on success */
static int join_one(const char *dir, const char *base) {
  char dst[768], tmp[800], part[800];
  struct stat st;

  snprintf(dst, sizeof dst, "%s/%s", dir, base);
  if (stat(dst, &st) == 0) return 1;          /* already joined */

  int nparts = 0;
  for (int i = 0; i < JOIN_MAX_PARTS; i++) {
    snprintf(part, sizeof part, "%s/%s.split%d", dir, base, i);
    if (stat(part, &st) != 0) break;
    nparts++;
  }
  if (nparts == 0) return 1;                  /* nothing to do */

  snprintf(tmp, sizeof tmp, "%s/%s.tmp", dir, base);
  remove(tmp);
  FILE *w = fopen(tmp, "wb");
  if (!w) { debugPrintf("[join] cannot create %s\n", tmp); return -1; }

  long long total = 0;
  for (int i = 0; i < nparts; i++) {
    snprintf(part, sizeof part, "%s/%s.split%d", dir, base, i);
    FILE *r = fopen(part, "rb");
    if (!r) { debugPrintf("[join] cannot read %s\n", part); fclose(w); remove(tmp); return -1; }
    for (;;) {
      size_t got = fread(g_join_buf, 1, JOIN_BUF, r);
      if (got == 0) break;
      if (fwrite(g_join_buf, 1, got, w) != got) {
        debugPrintf("[join] short write on %s (SD full?)\n", tmp);
        fclose(r); fclose(w); remove(tmp); return -1;
      }
      total += (long long)got;
    }
    fclose(r);
  }
  if (fclose(w) != 0) {
    debugPrintf("[join] fclose failed on %s (SD full?)\n", tmp);
    remove(tmp); return -1;
  }

  if (!join_size_ok(tmp, total)) { remove(tmp); return -1; }

  if (rename(tmp, dst) != 0) {
    debugPrintf("[join] rename %s -> %s failed\n", tmp, dst);
    remove(tmp); return -1;
  }
  debugPrintf("[join] %s <- %d part(s), %lld bytes\n", base, nparts, total);

#if JOIN_DELETE_PARTS
  for (int i = 0; i < nparts; i++) {
    snprintf(part, sizeof part, "%s/%s.split%d", dir, base, i);
    remove(part);
  }
  debugPrintf("[join] removed %d part file(s) for %s\n", nparts, base);
#endif
  return 0;
}

/* Scan `dir` for *.split0 and join each set. Safe to call on every boot: once
 * the base files exist this costs one readdir pass. */
void nx_join_split_assets(const char *dir) {
  DIR *d = opendir(dir);
  if (!d) { debugPrintf("[join] cannot open %s\n", dir); return; }

  static char bases[JOIN_MAX_SETS][256];
  int nb = 0;
  struct dirent *e;
  while ((e = readdir(d)) != NULL && nb < JOIN_MAX_SETS) {
    size_t len = strlen(e->d_name);
    if (len < 8 || len >= sizeof bases[0]) continue;
    const char *tail = e->d_name + len - 7;   /* ".split0" */
    if (strcmp(tail, ".split0") != 0) continue;
    memcpy(bases[nb], e->d_name, len - 7);
    bases[nb][len - 7] = '\0';
    nb++;
  }
  closedir(d);

  if (nb == 0) return;                        /* nothing split -- normal case
                                                 after the first boot */
  int joined = 0, failed = 0, already = 0;
  for (int i = 0; i < nb; i++) {
    int rc = join_one(dir, bases[i]);
    if (rc == 0) joined++;
    else if (rc == 1) already++;
    else failed++;
  }
  if (joined || failed)
    debugPrintf("[join] %d set(s) found: %d joined, %d already present, %d failed\n",
                nb, joined, already, failed);
}
