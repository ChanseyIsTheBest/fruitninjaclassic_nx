/* config.h -- Plants vs Zombies Fusion 3.6.1 Switch wrapper configuration
 * (forked from the Zookeeper DX / CR3 wrapper config.)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* ============================ MEMORY LAYOUT ==============================
 * These are engine-fitting parameters, not game content -- identical to the
 * Zookeeper DX port because Fruit Ninja Classic + is the SAME Unity minor version
 * (2022.3.62) and we apply the SAME 256MB->64MB region-granularity patch
 * (see nx_patch_fruitninja.h). Do not change unless you know the allocator math.
 * ======================================================================== */

// The engine + libc++ + il2cpp heap need a generous newlib heap; the rest of
// system memory is handed to the .so loader (see __libnx_initheap).
#define MEMORY_MB 768

// Anonymous-mmap arena. Unity reserves big region-aligned pools by over-mmapping
// then munmapping the unaligned head/tail. We back anonymous mmaps from a
// dedicated, region-aligned arena with a per-page used-bitmap so sub-range
// munmap frees exactly the trimmed pages. Region granularity is 64MB to match
// the libunity patch.
#define MMAP_ARENA_ALIGN    ((size_t)64 * 1024 * 1024)    // 64MB region granularity (libunity patched 256MB->64MB, nx_patch_fruitninja.h). NOTE: 16MB was tried and CORRUPTED Unity's Dynamic Heap allocator at init (overlapping regions from the over-map/trim pattern -> null-prev free-list crash). 64MB is the known-good floor.
/* Round 152: 192 -> 896 MB. The r151 log finally produced the failure this
 * comment has predicted for dozens of rounds, with the exact recorded
 * signature:
 *
 *   [oc] ARMED: window 704 MB          <- only 704 this boot; 1152-1600 in others
 *   [mem] arena 81% reserved (157 of 192 MB)
 *   [mmap] OC window full for 319 MB -> heap-backed arena
 *   [mmap] arena full -> newlib fallback (#1) -- memory pressure
 *   [mmap] 127 MB (prot=0x0 anon=1) -> 0x0     <- mmap returned NULL
 *   [mmap] arena full -> newlib fallback (#2) -- memory pressure
 *   -> crash in libunity
 *
 * "couldn't fit a 127MB OC-window overflow -> mmap NULL -> Unity crash" is
 * word for word what 512 MB did. We were running 192.
 *
 * It also explains the intermittency: the OC window is clamped to the largest
 * stack-region hole, which is different every boot (704 MB here, 1152-1600 in
 * runs that were fine). A big window absorbs the spill; a small one pushes it
 * into this arena, which at 192 MB could not take it. "A couple of perfect runs
 * then a crash" is that lottery.
 *
 * 896 is the value this comment has recommended all along = 1.75x the 512 fail
 * point. Anything at or below 512 is KNOWN to fail -- do not "compromise" at
 * 400. Paid for by right-sizing OC_POOL_BYTES below. */
#define MMAP_ARENA_RESERVE  ((size_t)896 * 1024 * 1024)

// Stack-region overcommit (OC) arena (see libc_shim.c): PROT_NONE reservations
// held in a stack-region window, committed pages backed from a small heap pool.
#define OC_WINDOW_BYTES     ((size_t)2048 * 1024 * 1024)  // 32x64MB cheap PROT_NONE reservation. Was 1536; the window-finder clamps to the largest stack-region hole (min(this, hole)), so raising the cap lets a run use its full hole and spill fewer reservations into the (real-memory) arena.
// Commit-pool: real memory backing touched pages of the OC window. Unity is told it
// has 512 MB (libc_shim.c __sysconf PHYS_PAGES + dalvik.vm.heapsize), and it reserves
// its heaps as big PROT_NONE regions that route here, so this pool must be able to
// back the full 512 MB Unity believes it has -- at 256 MB the scene load exhausted it
// (~270 MB working set) and the next uncommitted page faulted -> hard OOM crash.
// malloc. UPDATE 2: trimming the pool 1280->1024 + arena 1024->512 fixed the malloc
// OOM (malloc 1409 -> game pushed past the null-buffer crash) but 512 starved the
// arena (see above). That round settled on pool 896 + arena 896 + malloc 1153.
//
// UPDATE 3 (round 107): those last two no longer describe the tree. The ACTUAL
// values are pool 512 and MMAP_ARENA_RESERVE 896 as of round 152. (Between
// r107 and r152 they were pool 1024 / arena 192, and neither comment said so --
// the recorded "balance" was fiction for 45 rounds. It is accurate again now.) Against the observed 2752 MB newlib heap that gives
// pool 1024 + arena 192 + so_region 160 = 1376 MB, leaving ~1376 MB for malloc.
// Known failure points, for reference when one of these OOMs again:
//   pool live 551 | arena fail 512 | malloc fail 641
// Note the arena is now BELOW its recorded failure point. It has been booting, so
// the enlarged OC window is evidently absorbing what used to spill there, but it
// is the next thing to look at if an allocation failure shows up in the arena.
// RAISED 896 -> 1024 (round 107). Fruit Ninja's live footprint is not PvZ's --
// 244 MB observed at ModeSelect here, but the out-of-memory crash happened later,
// in play, and this is the pool that has to absorb it. +128 MB comes out of the
// plain-malloc share, which currently sits ~1376 MB against a recorded failure
// point of 641 MB, so there is room to give.
//
// Safe to raise now: main.c retries in 128 MB steps down to OC_POOL_MIN_BYTES if
// the allocation does not fit, and logs what it settled on. Previously a pool
// that was too big meant OC DISABLED and a dead boot, which is why this number
// had not been touched.
/* Round 152: 1024 -> 512 MB, to pay for the arena above without asking newlib
 * for more in total. 512 is this comment's own documented floor -- "must be
 * able to back the full 512 MB Unity believes it has" -- and the r151 session
 * peaked at `[oc] committed 235 MB (pool 235/1024)`, so it never used even half
 * of 512. The pool was over-reserved by ~4x while the arena starved.
 *
 * Budget against the observed 2752 MB newlib heap:
 *     before  pool 1024 + arena 192 = 1216   -> malloc ~1536, arena OOM'd
 *     after   pool  512 + arena 896 = 1408   -> malloc ~1344, still above the
 *                                               1153 the history settled on
 * If a scene ever pushes the pool past 512 the symptom is a hard OOM on an
 * uncommitted page (see above), and the fix is to take it back off the arena. */
#define OC_POOL_BYTES       ((size_t)512 * 1024 * 1024)   // commit-pool (touched pages only)
#define OC_POOL_MIN_BYTES   ((size_t)640 * 1024 * 1024)   // ladder floor; below the
                                                          // 551 MB live high-water there
                                                          // is no point continuing

// Overcommit (alias-region) mode: reserve a big *virtual* window (PROT_NONE
// costs only address space) and commit physical pages on demand -- true
// overcommit, matching Android.
#define MMAP_VIRT_RESERVE   ((size_t)6144 * 1024 * 1024)  // 6 GB virtual reservation window
#define OVERCOMMIT_HEAP_MB  608u                          // newlib malloc + .so load zone

/* ============================ GAME IDENTITY =============================== */

// Fruit Ninja Classic + ships the engine as the standard modern Unity trio; libmain.so
// dlopens libunity.so which dlopens libil2cpp.so. (No libcrx/MVGL here -- this
// is a normal IL2CPP game, so main.c loads libmain/libunity/libil2cpp directly
// and these SO_NAME macros are unused, kept only for parity with the base.)
#define SO_NAME      "libunity.so"
#define SO_CPP_NAME  "libil2cpp.so"

// The SD-card folder holding the .nro + the game files.
#define GAME_FOLDER  "fruitninja"

/* Bump when shipping. Printed at compile time (#pragma message in main.c)
 * and at boot, so a stale source tree is obvious from either the build
 * output or debug.log. */
#define FN_SRC_REV   "r152"

/* ---- Android package name -- VERIFY THIS AGAINST YOUR APK -----------------
 * Returned by our fake getPackageName(). Unity surfaces it as
 * Application.identifier, and game code (and any SDK that keys off it) can
 * read it.
 *
 * NOTE: the PvZ tree inherited Zookeeper's "jp.kiteretsu.zookeeper_dx" here and
 * never changed it -- both of those ports shipped reporting the WRONG package
 * name. Do not repeat that.
 *
 * The package name is not present in libunity/libil2cpp; it lives in the APK's
 * AndroidManifest.xml. Read it from your own copy:
 *     aapt dump badging FruitNinja.apk | head -1
 * and put the exact string here. The default below is the commonly published
 * id for Fruit Ninja Classic and is a PLACEHOLDER -- the "Classic +" / Play
 * Pass build may differ.                                                    */
#define GAME_PACKAGE "com.halfbrick.fruitninja"   /* <-- CHECK YOUR MANIFEST */

/* ---- split-asset auto-join (nx_splitjoin.c) ------------------------------
 * This build ships some assets as 1 MiB .split0/.split1/... parts, which Unity
 * reassembles in Java on a phone. We have no Java, so the loader joins them on
 * the SD card at first boot. Set to 1 to delete the .splitN parts once a join
 * has been verified -- saves ~8 MB, but means re-copying from the APK if you
 * ever want them back. Off by default: we do not delete user data uninvited. */
#define JOIN_DELETE_PARTS 0

/* Diagnostic: trace futex WAIT/WAKE in the contended allocator region to
 * locate a lost wake. Verbose; enabled for this bring-up build only. */
#define FN_FUTEX_TRACE 0

#define CONFIG_NAME "config.txt"
#define LOG_NAME    "sdmc:/switch/" GAME_FOLDER "/debug.log"

// Returned for getenv("HOME")/getpwuid()->pw_dir. Point it at the (writable)
// game data root instead of letting the engine deref a NULL passwd.
#define GAME_HOME   "sdmc:/switch/" GAME_FOLDER
#ifndef DATA_ROOT
#define DATA_ROOT   "sdmc:/switch/" GAME_FOLDER
#endif

// flip to 1 (and rebuild) to get file logging (debug.log) for on-hardware debugging
/* File logging.
 *
 *   1 = ON  [shipped]. debug.log is written normally: boot trace, per-frame
 *           diagnostics, [gc]/[mem] notes and the [xd] crash dump.
 *
 *   0 = ABSOLUTE SILENCE. debug.log is never created -- not by a crash, not by
 *       a "note", not at all. Every logging entry point compiles to an empty
 *       stub, so there is no file, no formatting cost and no SD traffic.
 *
 * Note that 0 does NOT disable the watchdog thread: it is also the escape hatch
 * that undoes a wedged GC stop-the-world (round 101), so it always runs. */
#define DEBUG_LOG 0

/* GC stop-the-world (round 100).
 *
 *   1 = CORRECT. Mutator threads are really paused while the collector marks.
 *       This is what a garbage collector requires, and without it the mark loop
 *       can read a half-published object and fault (round 99: klass == 0).
 *       The cost is real: the game's worker threads are stopped for the whole
 *       mark, so a large collection shows up as an occasional frame hitch.
 *
 *   0 = OLD BEHAVIOUR. Ack the suspend without pausing anyone. Smooth, and
 *       wrong -- this is the configuration that crashed ~20% of the time when
 *       starting a new game.
 *
 * Left ON: an occasional hitch is a better failure than a crash. Flip it to 0
 * if you would rather have the old behaviour back. */
#define FN_GC_STOP_WORLD 1

/* Strip "gc-max-time-slice" from boot.config as it is served to the engine.
 *
 * That key puts Unity's collector in INCREMENTAL mode: marking is split across
 * many short slices with the mutators running in between, and the invariant is
 * held together by write barriers plus a correct stop-the-world for each slice.
 * This port's stop-the-world is not reliable enough for that -- it works for ~32
 * collections in 33 and bails on the rest (round 110) -- and every crash so far
 * has landed in the mark loop reading a klass of 0, which is what a broken
 * incremental invariant looks like.
 *
 * With the key removed the collector runs non-incremental: fewer, larger, atomic
 * collections, with no between-slice invariant to violate. Expect occasional
 * longer pauses in exchange.
 *
 * 0 restores the game's shipped setting. */
#define FN_GC_NON_INCREMENTAL 1

/* High-volume per-operation traces. These were invaluable for the black-screen /
 * boot-hang triage but are catastrophic for load speed once the game runs: every
 * data.unity3d read/lseek and most mprot calls fflush two lines to the SD card, so
 * a synchronous scene load (~1700 bundle reads) takes minutes instead of seconds
 * and looks like a hang. Keep them OFF for normal play; flip to 1 to re-trace. */
#define TRACE_BUNDLE_IO 0   /* per-read/lseek trace of globalgamemanagers */
#define TRACE_MPROT     0   /* per-mprotect commit trace */

/* Per-frame / per-asset traces that dominate the log once the game runs:
 * the doFrame proxy line (every frame), [io] open (every asset, ~3x), and
 * the clock-stall beacon. Off = a readable log and far less SD I/O; flip to
 * 1 only to re-trace JNI proxy dispatch or asset open order. */
#define LOG_VERBOSE 0

/* Mutex ownership tracker (self-deadlock / holder naming). It takes a global
 * lock on every pthread_mutex op, which serialises the engine's mutex
 * traffic and reorders acquisition -- fine for a one-off deadlock hunt, but
 * it must be OFF for normal runs or it changes timing enough to hang boot.
 * Flip to 1 only to re-diagnose a mutex deadlock. */
#define MTXOWN_ENABLE 0

/* Force glFinish() before eglSwapBuffers on the first N presents, to work
 * around / localise the first-present hang (mesa blocking in its flush/
 * fence phase). 0 disables. A small number (a few frames) is enough to get
 * past the initial present without serialising steady-state rendering. */
/* round 63: adaptive futex re-poll floor. 250us gives fast recovery of
 * silent-writer waits (the async-load bottleneck) while backoff keeps idle
 * threads cheap. Lower = faster loads but more wakeups. */
#define FN_FUTEX_HOP_MIN 250000ULL
/* round 70: re-poll CEILING. Handoffs whose wake never arrives directly cost
 * one tick of this, so it sets the load speed. 16ms was the old value and is
 * why loading crawled; 1ms is ~16x faster. Lower = faster loads, more CPU. */
#define FN_FUTEX_HOP_MAX 1000000ULL
/* round 64: vsync/Choreographer pulse period. 16ms == ~60fps cap; the load
 * is frame-gated so a shorter period renders (and loads) faster. Delta-time
 * is hooked so game speed is unchanged. */
#define FN_VSYNC_PERIOD_NS 16000000ULL
#define FN_SWAP_FINISH_N 8

/* ---- libil2cpp hook gates (see patches/patch_sources.py) -----------------
 * libil2cpp is GAME CODE: every offset into it is specific to one build of one
 * game. The PvZ core hardcodes hooks at PvZ's offsets. Both are OFF here because
 * neither was re-derived for Fruit Ninja; turning one on without re-deriving it
 * first will patch unrelated functions. Symptoms and method: PORTING sec 6.    */
#define FN_HAVE_TIME_HOOKS 1   /* DERIVED from Il2CppDumper; verify-first per site */
#define FN_FORCE_SPLASH_FINISH 1  /* round 57: skip the stuck end-of-fade animation-event gate */
/* round 68: async-load integration budget, ms per frame. Unity default is
 * 4ms (High would be 50). Bigger = faster scene loads, fewer frames during
 * loading. 0 disables the patch. */
#define FN_PRELOAD_BUDGET_MS 4
/* round 75: NOP UpdatePreloading's early-exit branch so the main thread keeps
 * retrying SingleStep for the whole budget instead of yielding the frame the
 * moment the integrate queue is briefly empty. 0 disables. */
#define FN_PRELOAD_NO_EARLY_EXIT 1
/* round 69: per-asset "[io] DATA open" trace. Costs an extra fstat plus a
 * log line for each of the ~2200 assets loaded, so keep it off by default. */
#define FN_TRACE_DATA_IO 0
/* round 79: build+mount the Subway-Surfers-style asset pack. First boot
 * packs assets/bin/Data into one file; later boots mount it. 0 = off. */
#define FN_ASSET_PACK 1
#define FN_BYPASS_UNITY_SPLASH 1  /* round 58: GetShouldShowSplashScreen->0 (native Unity splash) */
/* JNI approximation ledger (round 130). Records every JNI call answered by a
 * catch-all -- empty string, NULL object, 0, no-op -- deduped and counted, and
 * marks the ones Unity reads back via ExceptionCheck. Android raises at this
 * boundary and we cannot; this is the half of that which costs nothing. The
 * ledger is reprinted in full on any crash dump, so a fault log now carries
 * "here is everything we faked" instead of needing a separate run.
 * Set to 0 for absolute silence (DEBUG_LOG 0 already suppresses the output). */
#define FN_JNI_LOUD 1

/* Poison freed newlib blocks with 0xDE (no quarantine, nothing retained).
 * Makes use-after-free deterministic instead of layout-dependent -- see the
 * comment in nx_alloc.c's nx_free_inner. Costs one memset per free; set to 0 to
 * restore the previous behaviour exactly. */
#define FN_POISON_FREE 1


/* Diagnostic ONLY: call il2cpp_gc_disable() after init so no collection ever
 * runs. Answers "is the corruption premature reclamation?" in one session.
 * Memory grows unbounded -- never ship with this set to 1. */
#define FN_GC_DISABLE 0

/* The on-device asset-pack payload verifier is GONE (round 142). It ran once,
 * printed "verify OK: payload matches header (b3e645de7a86ae28)", and that is
 * recorded -- it had no business costing a 255 MB blocking read on the boot
 * path. The same check now lives in tools/verify_pack.py, run on a PC. */

#define FN_HAVE_GC_BRIDGE  1   /* DERIVED -- 4 offsets from this libil2cpp (see nx_patch) */

extern int screen_width;
extern int screen_height;

/* ----------------------------- Language ----------------------------------
 * NOT INHERITED FROM PvZ -- re-derived for this game, and it works differently.
 *
 * PvZ's build read its locale as a STRING from an AndroidJavaClass, so its
 * config mapped 1/2 onto the literals "en"/"zh" returned by jni_fake's
 * getLanguage(). Fruit Ninja does not do that. Its managed code calls
 *
 *     UnityEngine.Application::get_systemLanguage()
 *
 * which returns Unity's SystemLanguage ENUM. The engine populates that at init
 * from the Java locale, so jni_fake's getLanguage() still feeds it -- but the
 * game compares against an enum, not against our string, so there is no
 * game-specific token to guess.
 *
 * Practical consequence: LANG_AUTO is the only value with confirmed meaning.
 * The overrides below simply force the locale jni_fake reports; verify on
 * hardware which SystemLanguage the engine derives before trusting them.
 *
 * (The binary also carries I18N.CJK/MidEast/Other/Rare/West and RTLTMPro, so
 * the title has broad language coverage including right-to-left. The ar-*
 * tokens visible in libil2cpp are mscorlib CULTURE TABLES from I18N.MidEast,
 * not a list of shipped game languages -- do not read them as one.)        */
#define LANG_AUTO 0   /* follow the Switch system language -- recommended */
#define LANG_EN   1   /* force en */
#define LANG_ZH   2   /* force zh -- retained from the base; UNVERIFIED here */

/* ORIENTATION -- UNRESOLVED, AND YOU MUST CHECK THIS BEFORE FIRST BOOT.
 *
 * PvZ removed the base's TATE/portrait path because that game is landscape-only.
 * We have NOT confirmed Fruit Ninja's orientation: its managed code contains no
 * ScreenOrientation manipulation and no landscape/portrait strings, so the APK's
 * AndroidManifest.xml (android:screenOrientation) is authoritative. Check it.
 *
 *   landscape -> nothing to do; this build matches PvZ and is correct as-is.
 *   portrait  -> you must restore the Zookeeper TATE compositor-rotation path,
 *                AND revisit nx_pointer, which maps the touch panel to render
 *                space with a straight stretch and applies no rotation. Aiming
 *                will be wrong otherwise -- silently, which is the worst kind.
 *
 * The `portrait` knob is retired here only because it is retired upstream; it
 * is NOT a statement that this game is landscape.                          */
typedef struct {
  int handheld_res;   /* render height in handheld mode: 720 or 1080 */
  int docked_res;     /* render height when docked:      720 or 1080 */
} Config;

extern Config config;

int read_config(const char *file);
int write_config(const char *file);

#endif
