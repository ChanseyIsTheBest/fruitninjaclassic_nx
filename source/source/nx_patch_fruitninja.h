/* nx_patch_fruitninja.h -- in-memory libunity.so patch table for
 * FRUIT NINJA CLASSIC +  (Unity 2022.3.0f1 / fb119bb0b476, arm64, IL2CPP).
 * Game binary BuildID xxHash 5af2f7f61687579a.
 *
 * WHAT IT DOES
 *   Unity's block allocator reserves memory in 256MB-aligned regions. On a 4GB
 *   Switch that granularity does not fit the so_loader address space, so we
 *   rewrite the allocator's region-size computation to 64MB granularity. Each
 *   entry rewrites one 32-bit instruction word: {from} is the stock word, {to}
 *   is the 64MB word. Same transform family as the Zookeeper DX / PvZ Fusion
 *   tables; only the offsets and registers differ.
 *
 * HOW THESE OFFSETS WERE DERIVED
 *   Not by word-search against another build's table (which is what PvZ had to
 *   do, and why 9 of its 21 sites were pinned only by adjacency). Instead:
 *
 *     1. A SYMBOLIZED reference build of the SAME Unity source revision
 *        (2022.3.0f1 / fb119bb0b476, BuildID 97db9dfe51b37bcb) was used to NAME
 *        the allocator functions:
 *           TLSAllocator<0>::ThreadInitialize
 *           BucketAllocator::BucketAllocator
 *           DynamicHeapAllocator::DynamicHeapAllocator
 *           MemoryManager::VirtualAllocator::MarkMemoryBlocks
 *           MemoryManager::VirtualAllocator::ReserveMemoryBlock
 *           MemoryManager::VirtualAllocator::GetMemoryBlockFromPointer
 *           MemoryManager::VirtualAllocator::GetBlockInfoFromPointer
 *           MemoryManager::GetAllocatorContainingPtr
 *           AtomicPageAllocator::AllocatePage
 *     2. Every instruction inside those functions that participates in the
 *        256MB computation was enumerated by DECODING operands (not by grepping
 *        for a constant): move-wide 0x10000000, UBFM immr=28, ADD/SUB lsl #28,
 *        and AND-immediate 0xFFFFFFFFF0000000. -> 17 sites.
 *     3. Each site was pinned into THIS binary by masked-context search: a
 *        window of surrounding instructions with the position-dependent fields
 *        (ADRP/ADR/B/BL/B.cond/CBZ/TBZ immediates) masked out. Every site
 *        resolved UNIQUELY, 16 of them at the minimum 6-instruction window.
 *     4. Verification: for all 17, the reference word and the game word are
 *        BYTE-IDENTICAL (marked '=' below) -- same opcode, same registers --
 *        and the game-side disassembly matches the reference instruction for
 *        instruction. Four of the addresses (0x3bf150, 0x3c0920, 0x3c2d08,
 *        0x3c51f0, 0x3c76d4) were also found independently by a raw scan for
 *        the 256MB move-wide encoding, which agrees.
 *
 *   Confidence: HIGH. This is a stronger derivation than the PvZ table it
 *   replaces, because the reference is the same source revision rather than
 *   62 patch releases away.
 *
 * TRANSFORMS (cross-checked against the PvZ 62f1c1 table -- our `from` words
 * 0xd35c9c2a, 0xcb0a7108 and 0xd35c9e89 are byte-identical to PvZ sites 12,
 * 16 and 20, and our computed `to` values match theirs exactly):
 *     MOVZ/MOVK   imm16 0x1000<<16 -> 0x0400<<16     256MB -> 64MB
 *     LSR  (UBFM) immr 28 -> 26, imms 63 unchanged   >>28 -> >>26
 *     UBFX (UBFM) immr 28 -> 26, imms -= 2           field WIDTH preserved
 *     SUB  shifted  lsl #28 -> lsl #26
 *     AND  bitmask  immr/imms += 2                   0xFFFFFFFFF0000000
 *                                                 -> 0xFFFFFFFFFC000000
 *
 * SAFETY
 *   nx_patch_libunity() is VERIFY-FIRST: it reads each target word and only
 *   patches if it already equals {from}; if ANY site mismatches it patches
 *   NOTHING and logs loudly. A wrong offset is caught, not catastrophic.
 *
 * BRANCH FORCES
 *   PvZ shipped with PVZ_HAVE_BRANCH_FORCES = 0 and two unlocated sites, on the
 *   theory that granularity alone may suffice. We are in a better position: the
 *   symbolized reference makes those functions nameable if they turn out to be
 *   needed. Leave this at 0 for first boot; if you hit an allocator abort under
 *   load, see PORTING_FRUITNINJA.md sec 5.
 */
#ifndef NX_PATCH_FRUITNINJA_H
#define NX_PATCH_FRUITNINJA_H

#include <stdint.h>

#define FN_HAVE_BRANCH_FORCES  0   /* not needed unless an allocator abort appears */
#define FN_HAVE_TIME_FIX       1   /* DERIVED -- entry/body/GetTimeManager all pinned (see below) */
#define FN_HAVE_FMOD_OPENSL    1   /* DERIVED -- output-select site pinned at 0x6e5308 */
/* ---- il2cpp IsInst null-klass guard (round 119) ---------------------------
 * DialogueConfig.m_dialoguePieces (+0x40) holds an Il2CppClass* instead of an
 * array object -- a partially-initialised class: image/gc_desc/name/namespaze
 * all NULL, element_class set. il2cpp's assignability check then does
 *
 *     ldr  x8, [x0]          ; obj->klass  == 0 here
 *     tbnz w8, #0, ...       ; falls through, bit0 clear
 *     and  x8, x8, #~1       ; NO-OP on this path (bit0 already clear)
 *     add  x9, x8, #0x135
 *     ldrh w21, [x9]         ; FAULT: reads 0x135
 *
 * An object whose klass is NULL is not an instance of anything, so returning 0
 * is correct, not a fudge. The `and` is provably dead on the fall-through path,
 * which frees exactly one slot for the guard.
 *
 * The branch target is a `mov w0,wzr` + the IDENTICAL epilogue
 * (ldp x20,x19,[sp,#0x10] / ldp x30,x21,[sp],#0x20 / ret) found elsewhere in
 * libil2cpp, so it unwinds this function's own frame exactly. 10 such sites lie
 * within CBZ's +-1 MB range; this is the nearest. */
#define FN_IL2CPP_ISINST_AND     0x159dfccu   /* the dead `and x8,x8,#~1`      */
#define FN_IL2CPP_ISINST_AND_OLD 0x927FF908u  /* expected encoding there       */
#define FN_IL2CPP_ISINST_GUARD   0xB4DAAC68u  /* cbz x8, #0x1553558            */
#define FN_HAVE_ISINST_GUARD     1

/* ---- DataBinding / ConvertFromTo frame map (round 131) --------------------
 * With the liveness crash guarded, ModeSelect loads and the next fault is the
 * long-standing open issue: a raw 1 reaching Object.GetType.
 *
 *   pc  libil2cpp+0x15b3974   ldr x8,[x0] ; add x0,x8,#0x20 ; b ...
 *                             = Object.GetType. A LEAF -- pushes nothing, so
 *                               sp at the fault is still ConvertFromTo's.
 *   lr  libil2cpp+0x183f1f8   IDataSource$$ConvertFromTo +0xe4, i.e. the
 *                             instruction after `bl 0x262487c` (a `b` thunk
 *                             straight to the GetType leaf above).
 *
 *   IDataSource.ConvertFromTo(object from, Type toType, string stringFormat,
 *                             int comparison, int compareTo)
 *     frame 0x60; x19=from x20=toType x24=stringFormat x21=comparison x22=compareTo
 *     saves x20,x19 at [sp+0x50] -- i.e. its CALLER's x20/x19.
 *
 *   DataBinding.PerformValueConversion(object value, Type to)
 *     frame 0x50; x20=__this x19=value.
 *
 * So at the fault: [sp+0x50] = the DataBinding, [sp+0x58] = the value.
 * Cross-check that validated this: [sp+0x58] equals the live x19 (both 1).
 *
 * DataBinding is reflection-driven (m_boundField/m_boundProperty are a
 * FieldInfo/PropertyInfo), and its three string fields say WHICH binding blew
 * up -- which is the thing twenty rounds of this fault have never once named. */
#define FN_IL2CPP_GETTYPE_LEAF    0x15b3974u  /* Object.GetType, leaf          */
#define FN_IL2CPP_CFT_LR          0x183f1f8u  /* ConvertFromTo +0xe4 (the lr)  */
#define FN_CFT_SP_DATABINDING     0x50u       /* [sp+0x50] = DataBinding this  */
#define FN_CFT_SP_VALUE           0x58u       /* [sp+0x58] = the value passed  */
#define FN_DATABINDING_PROPNAME   0x20u       /* string m_propertyName         */
#define FN_DATABINDING_DATAPATH   0x28u       /* string m_dataPath             */
#define FN_DATABINDING_STRFMT     0x38u       /* string m_stringFormat         */
#define FN_REFLECTIONTYPE_TYPE    0x10u       /* Il2CppReflectionType.type     */

/* Round 133 -- the rest of the stack, and the thing it proves.
 * Walking the saved x30s out of the same frames named the origin, which r131
 * said was not statically reachable (SourceValueChanged is a virtual/delegate
 * target with no callers in the binary). It is reachable through the STACK:
 *
 *   [sp+0x10]  -> DataBinding$$PerformValueConversion +0xa4
 *   [sp+0x70]  -> DataBinding$$SourceValueChanged     +0x238
 *   [sp+0xb0]  -> Dialogue$$SetDataSourceValues       +0x164
 *
 * and SetDataSourceValues +0x160 is `bl DataValue<object>$$SetValue` with
 * x1 = [x19+0x10], where x19 = Dialogue$$GetCurrentDialoguePiece(). That method:
 *
 *   ldr w8,[x19,#0x30]   Dialogue.m_currentDialogueIndex
 *   ldr x9,[x19,#0x20]   Dialogue.m_dialogueConfig
 *   ldr x9,[x9,#0x40]    DialogueConfig.m_dialoguePieces   <-- r119 / r129's field
 *   ldr x0,[x8,#0x20]    m_dialoguePieces[i]
 *
 * So the ModeSelect liveness crash and the ConvertFromTo crash are the SAME
 * bug seen from two directions. The value that reaches GetType is
 * DialoguePiece.m_characterIcon (+0x10) -- a `Sprite` reference field holding 1.
 * The piece object itself is real (heap-shaped pointer); only the reference
 * field inside it is a raw non-object.
 *
 * Frames, all 0x-sized from the fault sp: CFT 0x60, PVC 0x50, SVC 0x20, SDS 0x50. */
#define FN_SDS_SP_DIALOGUE        0x110u     /* [sp+0x110] = Dialogue __this     */
#define FN_SDS_SP_PIECE           0x118u     /* [sp+0x118] = the DialoguePiece   */
#define FN_DIALOGUE_CONFIG        0x20u      /* Dialogue.m_dialogueConfig        */
#define FN_DIALOGUE_INDEX         0x30u      /* Dialogue.m_currentDialogueIndex  */
#define FN_DLGCONFIG_PIECES       0x40u      /* DialogueConfig.m_dialoguePieces  */
#define FN_IL2CPP_ARRAY_LEN       0x18u      /* Il2CppArray.max_length           */
#define FN_IL2CPP_ARRAY_DATA      0x20u      /* first element                    */

/* ---- il2cpp liveness typeHierarchy guard (round 129) ----------------------
 * The r119 guard rejects `klass == NULL`. That is ONE garbage value, not the
 * bug: `DialogueConfig.m_dialoguePieces` (+0x40) still holds a non-object, and
 * when its first word happens to be non-NULL the guard passes and the very next
 * dereference faults instead:
 *
 *     ldr  x8,[x8,#0xc8]     ; klass->typeHierarchy == NULL
 *     add  x8,x8,x10,lsl#3   ; x10 = filter depth = 2
 *     ldur x8,[x8,#-8]       ; reads 0 + 16 - 8  ->  far == 0x8   (observed)
 *
 * There is no free slot for a second in-place test: all nine instructions of
 * the inlined HasParent are live, and libil2cpp contains no padding anywhere
 * (checked: zero runs of >=8 zero words, >=6 nops, >=6 brks in .text and in the
 * il2cpp section -- none). So the guard goes in front of the whole function.
 *
 * `hook_arm64()` cannot be used directly: it stores into module text, which is
 * mapped RX by so_finalize(). The same 16-byte thunk is written through
 * so_patch_code()'s writable alias instead -- the r120 lesson.
 *
 * The veneer re-tests what the clobbered prologue tested, adds the
 * typeHierarchy test on the filter path ONLY, then replicates the four
 * clobbered instructions and enters the untouched body at +0x10. It does not
 * touch the has_references path: that path never dereferences typeHierarchy, so
 * widening the guard to cover it would be exactly the over-broad gate of r124
 * and r127. */
#define FN_HAVE_LIVENESS_GUARD   1
#define FN_IL2CPP_LIVENESS_ADD   0x159dfb4u  /* add_process_object entry      */
#define FN_IL2CPP_LIVENESS_BODY  0x159dfc4u  /* body, past the hooked 16 bytes */
/* +0x58 = "report this object as ALIVE": ldr x0,[x20] (state->list) ; bl the
 * list-add ; then orr #1 into obj->klass to mark it. Round 155 jumps here when
 * the HasParent test cannot be performed -- see the veneer. */
#define FN_IL2CPP_LIVENESS_ALIVE 0x159e00cu  /* add_process_object+0x58        */
/* The four words the thunk overwrites, taken from the binary, not assumed:
 *   stp x30,x21,[sp,#-0x20]! / stp x20,x19,[sp,#0x10] / cbz x0,+0xa8 / ldr x8,[x0] */
static const uint32_t FN_LIVENESS_PROLOGUE[4] = {
  0xA9BE57FEu, 0xA9014FF4u, 0xB4000540u, 0xF9400008u
};

/* ---- il2cpp LivenessState walk: frame map for the dumper (round 129) ------
 * The round-119 guard sits inside il2cpp's `add_process_object`, and the crash
 * that came back faults 0x34 further into the SAME function. Disassembled:
 *
 *   0x159dfb4  add_process_object(obj, state)      frame 0x20
 *              stp x30,x21,[sp,#-0x20]! ; stp x20,x19,[sp,#0x10]
 *              cbz  x0, ret                       ; !object
 *              ldr  x8,[x0] ; tbnz w8,#0, ret     ; IS_MARKED
 *              and  x8,x8,#~1                     ; <- r119 guard slot
 *              ldrh w21,[x8+0x135] ; tbnz w21,#5  ; klass->has_references
 *              ldr  x9,[x20,#8]                   ; state->filter
 *              ldrb w11,[x8,#0x130]               ; obj klass depth
 *              ldrb w10,[x9,#0x130]               ; filter depth
 *              cmp  w11,w10 ; b.lo ret            ; HasParent, inlined
 *              ldr  x8,[x8,#0xc8]                 ; klass->typeHierarchy
 *              add  x8,x8,x10,lsl#3
 *   0x159e000  ldur x8,[x8,#-8]                   ; <- THE NEW FAULT
 *
 *   0x159dd08  traverse_object_instance(obj, state)   frame 0x30
 *              str x30,[sp,#-0x30]! ; stp x22,x21,[sp,#0x10] ; stp x20,x19,[sp,#0x20]
 *              x22 = word index 0..61 over klass->gc_desc taken as a bitmap
 *              bl add_process_object at 0x159dd3c  -> lr 0x159dd40
 *
 * add_process_object saves only x30/x21/x20/x19, so at a fault inside it:
 *   x22                    is STILL the caller's field word index
 *   [sp+0x40] / [sp+0x48]  are the caller's saved x20 (container) / x19 (state)
 * That is how the container gets named. Round 119 only had it by luck, because
 * the fault was shallow enough that x20 still held the container. */
#define FN_IL2CPP_LIVENESS_ADD_LO   0x159dfb4u  /* add_process_object entry     */
#define FN_IL2CPP_LIVENESS_ADD_HI   0x159e06cu  /* its final `ret`              */
#define FN_IL2CPP_LIVENESS_HASPAR   0x159e000u  /* the typeHierarchy[] load     */
#define FN_IL2CPP_LIVENESS_LR_INST  0x159dd40u  /* return addr into traverse_object_instance */
#define FN_IL2CPP_KLASS_TYPEHIER    0xc8u       /* Il2CppClass.typeHierarchy    */
#define FN_IL2CPP_KLASS_DEPTH       0x130u      /* Il2CppClass.typeHierarchyDepth */

/* The ConvertFromTo guard that lived here is REMOVED (round 127). It rejected
 * `from <= 0xfff`, which caught the crashing value 1 -- and also every ordinary
 * small int and bool the player-data deserializer converts, so save loading
 * failed wholesale and the game crashed MORE, as measured. The real fault was
 * upstream: Application.persistentDataPath was never a path at all. See the
 * Context path getters in jni_fake.c. */

#define FN_HAVE_FINISH_PROBE   0   /* il2cpp finish-flag diagnostic (PvZ offset) */

typedef struct { uint32_t off, from, to; } NxPatchWord;

/* ---- 17 region-granularity sites (256MB -> 64MB) : recovered, HIGH conf. ---
 * '=' : reference word and game word are byte-identical.                     */
static const NxPatchWord FN_PATCH_WORDS[] = {
  /*  0 = */ { 0x3bf14c, 0xd35cfc28, 0xd35afc28 },  /* TLSAllocator<0>::ThreadInitialize          +0x10  ubfm immr 28->26 imms 63->63 */
  /*  1 = */ { 0x3bf150, 0x52a20009, 0x52a08009 },  /* TLSAllocator<0>::ThreadInitialize          +0x14  movz 0x1000->0x0400 */
  /*  2 = */ { 0x3c0920, 0x52a20009, 0x52a08009 },  /* BucketAllocator::BucketAllocator           +0xf0  movz 0x1000->0x0400 */
  /*  3 = */ { 0x3c2d04, 0xd35cfd29, 0xd35afd29 },  /* DynamicHeapAllocator::DynamicHeapAllocator +0x98  ubfm immr 28->26 imms 63->63 */
  /*  4 = */ { 0x3c2d08, 0x52a2000a, 0x52a0800a },  /* DynamicHeapAllocator::DynamicHeapAllocator +0x9c  movz 0x1000->0x0400 */
  /*  5 = */ { 0x3c515c, 0xd35cfc33, 0xd35afc33 },  /* VirtualAllocator::MarkMemoryBlocks         +0x10  ubfm immr 28->26 imms 63->63 */
  /*  6 = */ { 0x3c5160, 0xd35cfd15, 0xd35afd15 },  /* VirtualAllocator::MarkMemoryBlocks         +0x14  ubfm immr 28->26 imms 63->63 */
  /*  7 = */ { 0x3c51f0, 0x52a20008, 0x52a08008 },  /* VirtualAllocator::ReserveMemoryBlock       +0x3c  movz 0x1000->0x0400 */
  /*  8 = */ { 0x3c552c, 0xd35cfc2c, 0xd35afc2c },  /* VirtualAllocator::GetMemoryBlockFromPointe +0x0   ubfm immr 28->26 imms 63->63 */
  /*  9 = */ { 0x3c553c, 0x92648c28, 0x92669428 },  /* VirtualAllocator::GetMemoryBlockFromPointe +0x10  bitmask immr 36->38 imms 35->37 */
  /* 10 = */ { 0x3c5544, 0xd35c9c2a, 0xd35a942a },  /* VirtualAllocator::GetMemoryBlockFromPointe +0x18  ubfm immr 28->26 imms 39->37 */
  /* 11 = */ { 0x3c559c, 0xcb0a7108, 0xcb0a6908 },  /* VirtualAllocator::GetMemoryBlockFromPointe +0x70  shift 28->26 */
  /* 12 = */ { 0x3c55b4, 0xd35cfc28, 0xd35afc28 },  /* VirtualAllocator::GetBlockInfoFromPointer  +0x0   ubfm immr 28->26 imms 63->63 */
  /* 13 = */ { 0x3c55c8, 0xd35c9c29, 0xd35a9429 },  /* VirtualAllocator::GetBlockInfoFromPointer  +0x14  ubfm immr 28->26 imms 39->37 */
  /* 14 = */ { 0x3c727c, 0xd35cfc28, 0xd35afc28 },  /* MemoryManager::GetAllocatorContainingPtr   +0xc   ubfm immr 28->26 imms 63->63 */
  /* 15 = */ { 0x3c7298, 0xd35c9e89, 0xd35a9689 },  /* MemoryManager::GetAllocatorContainingPtr   +0x28  ubfm immr 28->26 imms 39->37 */
  /* 16 = */ { 0x3c76d4, 0x52a20000, 0x52a08000 },  /* AtomicPageAllocator::AllocatePage          +0x48  movz 0x1000->0x0400 */
};
#define FN_PATCH_WORDS_N ((int)(sizeof(FN_PATCH_WORDS)/sizeof(FN_PATCH_WORDS[0])))

/* ---- branch forces: none located, and none needed for first boot --------- */
static const NxPatchWord FN_BRANCH_FORCES[] = { { 0, 0, 0 } };  /* placeholder */
#define FN_BRANCH_FORCES_N 0

/* ---- il2cpp JavaVM globals (the PvZ sec6b crash fix, re-derived) ----------
 * libil2cpp's own JNI_OnLoad (0x1588e1c in THIS build) must not be called: its
 * first action is a log through a GOT slot the loader mis-binds. Its essential
 * effects are two .bss stores, replicated by main.c:
 *
 *   0x01588e1c  mov  x19, x0            ; x19 = JavaVM*
 *   0x01588e38  bl   0x30b4080          ; <- the unsafe log call we skip
 *   0x01588e44  add  x0, x0, #0xe60     ; x0 = il2cpp+0x1588e60 (handler fn)
 *   0x01588e48  str  x19, [x8, #0xe80]  ; g_vm    = vm   -> +0x34f1e80
 *   0x01588e4c  bl   0x15a5ee8          ; setter: str x0,[x8,#0x730] -> +0x34f2730
 *
 * Both targets verified to lie in libil2cpp's .bss (writable). Structurally
 * identical to PvZ's +0x3c09c18 / +0x3c0abe8 pair; the VALUES are ours.       */
#define FN_IL2CPP_VM_GLOBAL       0x34f1e80  /* g_javavm            (.bss) */
#define FN_IL2CPP_HANDLER_SLOT    0x34f2730  /* g_jni_handler_fnptr (.bss) */
#define FN_IL2CPP_HANDLER_FN      0x1588e60  /* value stored into the slot */
#define FN_HAVE_IL2CPP_VM         1

/* ---- FMOD -> OpenSL sites (NOT yet derived; see PORTING_FRUITNINJA.md sec 6)
 * AudioManager::InitFMOD is pinned at game RVA 0x6e49a4 (ref 0xb406dc, 2184
 * bytes, 100% opcode-shape match over the prologue and 19/19 immediate votes).
 * The engine struct field PvZ keyed off -- [x19,#0x158] -- is present at
 * ref +0x290 / game 0x6e4c34, confirming the FMOD System layout is unchanged.
 * What differs is the surrounding code: this build has no
 * `cmp/mov #0x15/mov #0x17/csel/.../mov w1,w21` output-type-select run, so
 * PvZ's signature does not apply directly and the correct site must be
 * identified rather than assumed. Both fmod_output_opensl.cpp and
 * fmod_output_audiotrack.cpp are compiled in, so the OpenSL path exists.
 * Audio being off does not block boot or video -- leave FN_HAVE_FMOD_OPENSL 0. */
static const NxPatchWord FN_FMOD_WORDS[] = { /* TODO */ };
#define FN_FMOD_WORDS_NUM 0

/* =========================================================================
 * TimeManager -- DERIVED FOR THIS GAME  (was PvZ's; now ours)
 * =========================================================================
 * TimeManager::Update(double) located by fuzzy masked-window scoring against
 * the symbolized 2022.3.0f1 reference (ref 0x7f7438, size 456):
 *
 *     score 39/40, runner-up 0, margin 39  -> CONCLUSIVE
 *
 * The prologue is byte-for-byte what main.c's nx_time_update_hook replays:
 *
 *     0x478aec  ldr  x8,  [x0, #0xc8]     frameCount   (u64)
 *     0x478af0  ldr  w9,  [x0, #0xd0]     aux counter  (u32)
 *     0x478af4  ldrb w10, [x0, #0xf8]     paused       (u8)
 *     0x478af8  add  x8, x8, #1
 *     0x478afc  add  w9, w9, #1
 *     0x478b00  str  x8,  [x0, #0xc8]
 *     0x478b04  str  w9,  [x0, #0xd0]
 *     0x478b08  cbz  w10, 0x478b10        -> body
 *     0x478b0c  ret
 *
 * so BODY = ENTRY + 0x24 (the cbz target), the same delta PvZ had. The struct
 * field offsets 0xc8 / 0xd0 / 0xf8 are unchanged from the 62f1c1 build, which
 * is why the existing hook works without modification.
 *
 * GetTimeManager() is `mov w0,#7 ; b <manager-array-getter>` -- a tail call
 * with subsystem index 7. Located by finding that exact 2-instruction pair in
 * the game (7 such pairs exist; only two target a plain
 * `adrp/add/ldr x0,[x8,w0,sxtw#3]/ret` array read, matching the reference's
 * lookup body, and 0x4790d4 is the first).                                   */
#define FN_TIME_UPDATE_ENTRY  0x478aec    /* TimeManager::Update entry        */
#define FN_TIME_UPDATE_WORD   0xf9406408u /* ldr x8,[x0,#0xc8] -- guard word  */
#define FN_TIME_UPDATE_BODY   0x478b10    /* ENTRY + 0x24, the cbz target     */
#define FN_TIME_GETMANAGER    0x4790d4    /* mov w0,#7 ; b <array getter>     */

/* =========================================================================
 * FMOD -> OpenSL  -- DERIVED FOR THIS GAME
 * =========================================================================
 * The output-type select is NOT in AudioManager::InitFMOD in this build; it is
 * in AudioManager::InitNormal (game 0x6e5234). PvZ's 9-word signature matches
 * there exactly:
 *
 *     0x6e52e8  cmp   w0, #3
 *     0x6e52ec  mov   w8, #0x15          21
 *     0x6e52f0  mov   w9, #0x17          23
 *     0x6e52f4  csel  w8, w9, w8, eq
 *     0x6e52f8  cmp   w0, #2
 *     0x6e52fc  mov   w9, #0x16          22 == OPENSL
 *     0x6e5300  csel  w21, w9, w8, eq
 *     0x6e5304  ldr   x0, [x19, #0x158]  FMOD system
 *     0x6e5308  mov   w1, w21            <-- PATCH SITE
 *     0x6e530c  ...
 *     0x6e5310  bl    setOutput
 *
 * The enum value is DERIVED, not assumed: the engine itself materialises
 * #0x16 (22) here as one of the three outputs it can select, bracketed by
 * 21 and 23 -- the same bracketing PvZ documented. Forcing w1 to 22 makes
 * setOutput always choose OpenSL, which our opensles.c shim backs.
 *
 * The guard word 0x2a1503e1 (`mov w1, w21`) is byte-identical to PvZ's.
 *
 * NOT DERIVED: the buffer-geometry bypass. PvZ additionally neutralised a
 * `b.ls` in FMOD's OpenSL init so buffer-size validation could not reject the
 * Switch's geometry. Its signature (sub/mul/cmp/b.ls/lsr/str [x19,#0x3f8]/
 * cmp/b.ls) does not appear in this build's FMOD -- only constructor-style
 * stores to +0x3f8 exist -- so the check is either absent or reshaped here.
 * Left disabled and verify-first. If debug.log shows OpenSL init failing on
 * buffer geometry, that is the thing to hunt.                                */
#define FN_FMOD_OUTPUT_SITE   0x6e5308    /* mov w1,w21 -> movz w1,#22        */
#define FN_FMOD_BUFFER_SITE   0xe11404    /* PvZ value -- NOT derived, gated  */
#define FN_HAVE_FMOD_BUFFER_BYPASS 0      /* see note above                   */

/* =========================================================================
 * UnityEngine.Time icall thunks -- DERIVED FOR THIS GAME (Il2CppDumper)
 * =========================================================================
 * Source: dump.cs / script.json produced by Il2CppDumper against THIS game's
 * libil2cpp.so + global-metadata.dat.
 *
 * RVA CONVENTION -- CHECK THIS IF YOU RE-DUMP. The often-quoted rule is
 * "runtime RVA = Offset + 0x4000". That is NOT true for this dump: measured
 * across 86,333 methods the delta is uniformly 0x1000, and dump.cs's own
 * "VA:" field equals its "RVA:" field. The values below are those VAs, which
 * is what the loader wants (il2cpp_mod.load_virtbase + RVA).
 *
 * VALIDATION: each address was confirmed by disassembling this binary at that
 * RVA and reading the icall signature string the thunk loads -- e.g. the body
 * at 0x2ea2788 loads "UnityEngine.Time::get_deltaTime()". All seven matched
 * their own name, which is only possible if the dump corresponds to this exact
 * libil2cpp. (The UnityEngine.Time class is TypeDefIndex 10049, annotated
 * [StaticAccessor("GetTimeManager()")].)
 *
 * Each thunk is a 40-byte lazy icall resolver beginning
 * `stp x30, x19, [sp, #-0x10]!` (0xa9bf4ffe) -- uniform across all seven, and
 * used as the verify-first guard. A 16-byte redirect stub fits comfortably.  */
#define FN_TIME_THUNK_WORD    0xa9bf4ffeu /* stp x30,x19,[sp,#-0x10]! -- guard */
#define FN_IL2_get_time                 0x2ea2738
#define FN_IL2_get_timeSinceLevelLoad   0x2ea2760
/* SplashScreen.<LoadStartupScene>d__17.MoveNext: ldrb w8,[x21,#0x48]=m_isSplashFinished
 * (word 0x394122a8). Patched to mov w8,#1 to skip the non-firing animation-event
 * gate (round 57). VA==RVA in this il2cpp (delta 0x1000 handled by loader). */
#define FN_IL2_SPLASH_FINISHED_CHECK    0x170dad8
/* Unity native splash gate: GetShouldShowSplashScreen() (matched from ref
 * _Z25GetShouldShowSplashScreenv). Called each frame by the player loop at
 * libunity+0x5f23b4; while nonzero the engine holds its splash. Patched to
 * return 0. Entry: str x30,[sp,#-0x10]! (0xf81f0ffe) ; bl (0x941aaa72). */
#define FN_GET_SHOULD_SHOW_SPLASH        0x42b4dc
/* PreloadManager::UpdatePreloading async-load budget writers (round 68). */
#define FN_PRELOAD_BUDGET_TABLE_LOAD     0x4aa1b0   /* ldr  w21,[x9,x8,lsl#2] */
#define FN_PRELOAD_BUDGET_DEFAULT        0x4aa1b8   /* movz w21,#4            */
#define FN_PRELOAD_EXIT_BRANCH           0x4aa1ec   /* tbz w0,#0 -> give up frame */
#define FN_IL2_get_deltaTime            0x2ea2788
#define FN_IL2_get_unscaledTime         0x2ea27b0
#define FN_IL2_get_unscaledDeltaTime    0x2ea27d8
#define FN_IL2_get_smoothDeltaTime      0x2ea2800
#define FN_IL2_get_timeScale            0x2ea2828
#define FN_IL2_get_frameCount           0x2ea2888
#define FN_IL2_get_realtimeSinceStartup 0x2ea1a58

/* ---- VideoPlayer: the PORTING sec 7 risk, now scoped -------------------- *
 * Only ONE game class touches VideoPlayer -- CreateRenderTextureForVideoPlayer
 * (TypeDefIndex 3268), a MonoBehaviour that sizes a RenderTexture to the video
 * and pushes it into a RawImage. It REACTS to prepareCompleted via
 * OnVideoPlayerReady(); it does not gate a boot sequence on it, and there is
 * no coroutine spinning on isPrepared. So the stubbed MediaNDK should degrade
 * to "video never appears" rather than "boot hangs".
 *
 * If it DOES hang there, these are the two handles you need. Neuter Prepare()
 * with a bare `ret` (0xd65f03c0), or redirect it to call OnVideoPlayerReady
 * yourself. Both are 16-byte-stub targets like the Time thunks.             */
#define FN_IL2_CRTFVP_Prepare            0x17333d4  /* CreateRenderTextureForVideoPlayer::Prepare() */
#define FN_IL2_CRTFVP_OnVideoPlayerReady 0x17335e4  /* ...::OnVideoPlayerReady(VideoPlayer) */
#define FN_HAVE_VIDEO_BYPASS 0   /* off: video is not expected to block boot */

/* =========================================================================
 * Still PvZ's -- verify-first, will SKIP
 * ========================================================================= */
/* ---- Swappy frame-pacing gate -- DERIVED FOR THIS GAME ------------------
 * `Swappy::IsEnabledAndActive()`, named in the symbolized 2022.3.0f1 reference
 * at 0x99da74 (size 96); fuzzy-pinned to game 0x5e2180 (19/24, runner-up 5).
 * Every mismatch in that score is a position-dependent field -- the adrp page
 * and the low-12 offset of the SAME global, plus branch targets. Four
 * independent confirmations, all matching PvZ's description of their site:
 *   1. prologue 0xa9bf4ffe (stp x30,x19,[sp,#-0x10]!), byte-identical ref/game
 *      and exactly the guard main.c already tests for
 *   2. 13 call sites in the game -- PvZ counted 13
 *   3. 12 of those are `bl <fn> ; tbz w0,#0` -- PvZ's stated pattern
 *   4. `ldrb w8,[x0,#0x42a]` byte-identical ref/game
 * Patched to `mov w0,#0 ; ret`. THIS IS THE FRAME-0 BOOT WALL: without it,
 * engine init parks in a pthread_join because Swappy's ChoreographerFilter
 * threads wait on an Android Choreographer that does not exist on Switch.
 * (PvZ's value here was 0x652354, which in this binary is a bare `ret`.)   */
#define FN_PACING_GETTER      0x5e2180   /* Swappy::IsEnabledAndActive() */

/* ---- Boehm GC stop-the-world bridge -- DERIVED FOR THIS GAME ------------
 * libil2cpp has exactly TWO callers of pthread_kill: Boehm's GC_suspend_all
 * and GC_start_world.
 *   0x160ec38  ldr w1,[x24,#0x684] -> il2cpp+0x34e4684  suspend signal
 *   0x160ee84  ldr w8,[x23,#0x680] -> il2cpp+0x34e4680  ack gate
 *   0x160eea0  ldr w1,[x24,#0x688] -> il2cpp+0x34e4688  restart signal
 * Three consecutive 4-byte globals at +0/+4/+8 -- the same layout PvZ found
 * at 0x3bfbd40/44/48.
 * The ack semaphore is il2cpp+0x37060c8 (.bss), confirmed two independent ways:
 *   sem_init  @0x160ef84 : adrp x0,#0x3706000 ; add x0,x0,#0xc8
 *   poll loop @0x160f7b8 : adrp x21,#0x3706000 ; add x21,x21,#0xc8
 * and it is that same x21 that Boehm passes to sem_getvalue at 0x160f834.
 *
 * CORRECTION -- this was 0x3706190 in the first hardware build, and that is
 * what hung it. The derivation script tracked adrp/add results by writing the
 * computed value back into the SOURCE register, so `add x0,x0,#0xc8` applied
 * twice: 0x3706000 + 0xc8 + 0xc8 = 0x3706190. The bridge then posted acks into
 * memory that sem_init had never touched, sem_post_fake saw a NULL slot and
 * silently no-opped, and Boehm's GC_stop_world sat in
 *
 *     usleep(3000) ; sem_getvalue(&ack_sem, &n) ; if (n == expected) break
 *
 * forever -- exactly the "UnityMain futex_spin, last frame=0" stall observed.
 * The other three offsets were correct: the log's "suspend sig=30" matches the
 * default 0x1e written to +0x684 at 0x160ef50.                             */
/* ---- managed-exception hook point (round 148, NOT yet installed) ----------
 * Every managed throw goes through il2cpp_raise_exception, a REAL exported
 * symbol (.dynsym @0x15ef8a8), so no derived offset can go stale. It is exactly
 * four instructions -- 16 bytes, one thunk's worth:
 *
 *   015ef8a8  str x30, [sp, #-0x10]!
 *   015ef8ac  mov x1, xzr
 *   015ef8b0  bl  #0x159c1cc
 *   015ef8b4  b   #0x159cbfc        (tail call: the actual raise)
 *
 * A veneer would log the exception's class name (x0 is the Il2CppException*,
 * klass at [x0], klass->name at klass+0x10) and then replicate those four --
 * both targets need indirect branches, being far outside our NRO.
 *
 * Why it matters: the r147 log has three NullReferenceExceptions, one of them
 * 68 lines before the crash, and Unity printed the message with NO stack trace.
 * A managed exception in il2cpp is raised as a C++ exception, so it unwinds and
 * runs cleanup landing pads -- and the r134 wild write faulted INSIDE a landing
 * pad. Naming the throws would connect those two directly. */
#define FN_IL2_RAISE_EXCEPTION   0x15ef8a8   /* exported; 4 insns, 16 bytes   */
#define FN_IL2_RAISE_INNER_BL    0x159c1cc   /* the bl it makes              */
#define FN_IL2_RAISE_TAIL        0x159cbfc   /* the b it tail-calls          */

/* ---- CORRECTION + additions, round 143 -------------------------------------
 * A sibling port of the same lineage (acpc_nx) implements this bridge properly,
 * and comparing against it settles two things.
 *
 * 1. NAMING. Its trio is GC_RETRY_SIGNALS / GC_SUSPEND_SIG / GC_RESTART_SIG at
 *    +0/+4/+8 -- the same layout we have. So +0x680 is **GC_retry_signals**,
 *    not a "restart ack gate". Confirmed in our own binary at 0x160eae0:
 *      ldr w8,[x8,#0x680] ; cbnz w8  -> post a SECOND ack on the retry path
 *    The old name is kept as an alias so nothing breaks, but the meaning is
 *    what matters: it decides whether restart must ack again.
 *
 * 2. WHAT WE ARE MISSING. Boehm's SIG_SUSPEND handler runs ON the mutator and
 *    (a) spills its registers somewhere scannable, (b) records its current SP in
 *    thread->stack_ptr, (c) posts the ack, (d) sets thread->last_stop_count.
 *    Horizon delivers no signals, so that handler never runs. Our
 *    pthread_kill_gc ignores its `t` argument entirely and bulk-pauses every
 *    thread on the first call -- the pause is real, but stack_ptr is never
 *    written, so GC_push_all_stacks scans a STALE range with no registers.
 *    Objects reachable only from a paused thread's live frame or a callee-saved
 *    register are invisible to the mark. That is premature reclamation, and it
 *    matches every corruption shape from r119 to r137.
 *
 * Everything needed to fix it is below, derived from OUR binary (not copied
 * from the sibling) and cross-checked against its layout:
 *
 *   GC_lookup_thread, 0x160ea98:
 *     lsr x9,x0,#8 ; eor w8,w9,w1 ; eor w8,w8,w8,lsr#16 ; and x8,x8,#0xff
 *     add x9,x9,#0xe8 ; ldr x21,[x9,x8,lsl#3]     <- GC_threads[256] @ +0xe8
 *     ldr x8,[x21,#8] ; ldr x21,[x21]             <- id @+8, next @+0
 *   GC_suspend_handler_inner, 0x160ea8c / 0x160eb10:
 *     add x8,x8,#0xb8 ; ldar x20,[x8]             <- GC_stop_count @ +0xb8
 *     str x8,[x21,#0x18]                          <- stack_ptr  @ +0x18
 *     stlr x20,[x21+0x10]                         <- last_stop_count @ +0x10
 *     orr x8,x20,#1                               <- the retry-path | 1
 *   GC_threads spans 0x800 (256 pointers); the next global is at 0x37068f0,
 *   i.e. exactly +0x808. That is what confirms the table size.
 *
 * NOTE the sibling casts pthread_t straight to a libnx `Thread*`. We cannot:
 * our pthread_t is newlib's, so the pthread -> Handle mapping has to come from
 * diag (diag_handle_for_pthread), and the capture uses svcSetThreadActivity +
 * svcGetThreadContext3 rather than threadPause + threadDumpContext.          */
#define GC_RETRY_SIGNALS_OFF_FN 0x34e4680 /* .data  GC_retry_signals          */
#define GC_STOP_COUNT_OFF_FN    0x37060b8 /* .bss   GC_stop_count (ldar)      */
#define GC_RESTART_SEM_OFF_FN   0x37060d8 /* .bss   handler waits here        */
#define GC_THREADS_OFF_FN       0x37060e8 /* .bss   GC_threads[256]           */
#define GC_THREAD_ID_OFF        0x08      /* GC_thread.id                     */
#define GC_THREAD_STOPCNT_OFF   0x10      /* GC_thread.last_stop_count        */
#define GC_THREAD_STACKPTR_OFF  0x18      /* GC_thread.stack_ptr  <- THE ONE  */

#define GC_START_ACK_OFF_FN   0x34e4680  /* alias of GC_RETRY_SIGNALS_OFF_FN */
#define GC_SUSPEND_SIG_OFF_FN 0x34e4684  /* .data  GC_suspend_all signal */
#define GC_RESTART_SIG_OFF_FN 0x34e4688  /* .data  GC_start_world signal */
#define GC_ACK_SEM_OFF_FN     0x37060c8  /* .bss   GC_suspend_ack_sem    */
#define FN_IL2CPP_FINISH_FLAG 0x21fd0718 /* il2cpp finish probe  -- NOT derived */

#endif /* NX_PATCH_FRUITNINJA_H */
