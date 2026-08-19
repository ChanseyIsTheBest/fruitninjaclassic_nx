/* unity_entrypoints.h -- UnityPlayer native methods recovered from
 * libunity.so's JNI_OnLoad  (FRUIT NINJA CLASSIC +, Unity 2022.3.0f1,
 * build fb119bb0b476, arm64 / IL2CPP).
 *
 * Auto-extracted from THIS build's libunity.so with
 * tools/extract_entrypoints.py.  JNI_OnLoad is at 0x60b2e0 and makes 11 bl
 * calls, 9 of which are RegisterNatives helpers.  Helper #1 registers the
 * 26 UnityPlayer natives below (the drive surface); the other 8 register
 * Swappy/Choreographer frame-pacing (ignored -- we run our own loop) and
 * stub SDK classes (ARCore / Camera2 / HFP / audio-volume / orientation-lock
 * / status-query) that this port never invokes.
 *
 * IMPORTANT: these offsets are LINK-TIME addresses for THIS EXACT libunity.so
 * (BuildID xxHash 5af2f7f61687579a). If the game is patched/updated, re-run
 *   python3 tools/extract_entrypoints.py libunity.so
 * and paste the new offsets here.
 *
 * Runtime address = unity_mod.load_virtbase + offset (the .so links at base 0).
 *
 * VERSION NOTE: the loader core this overlays (Zookeeper DX / PvZ Fusion) was
 * built against Unity 2022.3.62.  This engine is 2022.3.0f1 -- same 2022.3 LTS
 * line, 62 patch releases earlier.  The UnityPlayer method SET is identical
 * except for two methods that 2022.3.0 does not have:
 *
 *   nativeHidePreservedContent  -- added later; declared but never called by
 *                                  the loader core, so it is simply dropped.
 *   nativeSendSurfaceChanged    -- the core already aliases this to
 *                                  nativeSendSurfaceChangedEvent, which exists.
 *
 * Both are handled below.  Every entry point main.c actually calls is present.
 */
#ifndef UNITY_ENTRYPOINTS_H
#define UNITY_ENTRYPOINTS_H

#include <stdint.h>
#include "so_util.h"

/* ---- UnityPlayer native method offsets (link-time vaddr) ---------------- */
/* JNI_OnLoad */
#define OFF_JNI_OnLoad                    0x60b2e0 /* (JavaVM*,reserved)->jint  caches VM, registers natives */

/* drive-critical */
#define OFF_initJni                       0x60a4bc /* (env,thiz,Context)                 */
#define OFF_nativeRecreateGfxState        0x60a70c /* (env,thiz,int,Surface)  set surface*/
#define OFF_nativeSendSurfaceChangedEvent 0x60a774 /* (env,thiz)                         */
#define OFF_nativeRender                  0x60a7cc /* (env,thiz)->Z   per-frame; false=stop */
#define OFF_nativeInjectEvent             0x60a82c /* (env,thiz,InputEvent,int)->Z input */
#define OFF_nativePause                   0x60a558 /* (env,thiz)->Z                      */
#define OFF_nativeResume                  0x60a5bc /* (env,thiz)                         */
#define OFF_nativeFocusChanged            0x60a69c /* (env,thiz,Z)                       */
#define OFF_nativeDone                    0x60a4c8 /* (env,thiz)->Z   shutdown           */
#define OFF_nativeApplicationUnload       0x60a64c /* (env,thiz)                         */
#define OFF_nativeLowMemory               0x60a604 /* (env,thiz)                         */
#define OFF_nativeOrientationChanged      0x60b228 /* (env,thiz,int,int)                 */

/* secondary / usually unused for a port */
#define OFF_nativeUnitySendMessage        0x60ae3c /* (env,thiz,String,String,byte[])    */
#define OFF_nativeMuteMasterAudio         0x60b04c /* (env,thiz,Z)                       */
#define OFF_nativeGetNoWindowMode         0x60b288 /* (env,thiz)->Z                      */
#define OFF_nativeIsAutorotationOn        0x60afec /* (env,thiz)->Z                      */
#define OFF_nativeSetLaunchURL            0x60b0f0 /* (env,thiz,String)                  */
#define OFF_nativeRestartActivityIndicator 0x60b0a8 /* (env,thiz)   -- not in the PvZ build */

/* soft keyboard (route via SoftInputProvider stub; not needed for first boot) */
#define OFF_nativeSetInputArea            0x60ab24
#define OFF_nativeSetKeyboardIsVisible    0x60aba4
#define OFF_nativeSetInputString          0x60abfc
#define OFF_nativeSetInputSelection       0x60ac9c
#define OFF_nativeSoftInputClosed         0x60adec
#define OFF_nativeSoftInputCanceled       0x60ad04
#define OFF_nativeSoftInputLostFocus      0x60ad54
#define OFF_nativeReportKeyboardConfigChanged 0x60ada4

/* ---- 2022.3.0f1 compatibility shims ------------------------------------ */
/* This engine has nativeSendSurfaceChangedEvent but not the later
 * nativeSendSurfaceChanged. The core only ever calls the *Event form; the
 * alias keeps any stray reference compiling. */
#define OFF_nativeSendSurfaceChanged      OFF_nativeSendSurfaceChangedEvent

/* nativeHidePreservedContent does not exist in 2022.3.0f1. The loader core
 * declares it but never calls it. Resolving it to 0 makes an accidental call
 * fail loudly instead of jumping into the middle of an unrelated function. */
#define OFF_nativeHidePreservedContent    0x0      /* ABSENT in 2022.3.0f1 -- do not call */
#define FN_HAVE_HIDE_PRESERVED_CONTENT    0

/* ---- JNI native signatures: ret (*)(JNIEnv*, jobject thiz, args...) ----- */
typedef void     (*fn_initJni)(void*,void*,void*);
typedef void     (*fn_gfxstate)(void*,void*,int32_t,void*);
typedef void     (*fn_v)(void*,void*);
typedef uint8_t  (*fn_z)(void*,void*);
typedef void     (*fn_vz)(void*,void*,int32_t);
typedef uint8_t  (*fn_inject)(void*,void*,void*,int32_t);
typedef void     (*fn_orient)(void*,void*,int32_t,int32_t);

#define UNITY_RESOLVE(mod, off) ((void*)((uintptr_t)(mod).load_virtbase + (off)))

/* ===========================================================================
 * Drive sequence (what the Java UnityPlayer does; you do it in main.c):
 *
 *   initJni(env, thiz, fake_context);                  // early init
 *   nativeRecreateGfxState(env, thiz, 0, fake_surface);// give it the surface
 *   nativeSendSurfaceChangedEvent(env, thiz);          // engine builds GL state
 *   for (;;) {
 *       // input: nativeInjectEvent(env,thiz, motionEvent, deviceId);
 *       if (!nativeRender(env, thiz)) break;           // false == engine wants out
 *   }
 *   nativeApplicationUnload(env, thiz);  nativeDone(env, thiz);
 *
 * NOTE on input: nativeInjectEvent takes a Java InputEvent/MotionEvent jobject,
 * which the engine then queries back via JNI (getActionMasked/getX/getY/
 * getPointerId/getPointerCount...). Fruit Ninja is a swipe game driven ENTIRELY
 * by this path -- it is the single most load-bearing subsystem in the port.
 * Sub-frame swipe sampling matters here in a way it did not for PvZ: the blade
 * trail is built from the MotionEvent history, so feeding one point per frame
 * gives coarse, "steppy" slices. See PORTING_FRUITNINJA.md sec 8.
 * =========================================================================== */

/* ---- Non-UnityPlayer native tables also present in this build (FYI) -------
 * We do NOT register/drive these; listed only so nobody re-hunts them.
 *   choreographer   nOnChoreographer                     @0xb458ec
 *   swappy          nOnRefreshPeriodChanged              @0xb47bec
 *                   nSetSupportedRefreshPeriods          @0xb47a0c
 *   ARCore          initializeARCore/pause/resume        @0x5e34e4/0x5e3548/0x5e359c
 *   Camera2         initCamera2Jni/deinit                @0x6035fc/0x603648
 *                   nativeFrameReady/SurfaceTextureReady @0x607b38/0x6079d0
 *   HFP audio       initHFPStatusJni/deinit              @0x5e61b0/0x5e61fc
 *   audio volume    onAudioVolumeChanged                 @0x5ec484
 *   query status    nativeStatusQueryResult              @0x5e53dc
 *   orient lock     nativeUpdateOrientationLockState     @0x5ec5c0
 *
 * (This build does NOT register nativeGetSoftInputType, which the 2022.3.62
 *  builds do. Nothing in the loader calls it.)
 * -------------------------------------------------------------------------- */

#endif /* UNITY_ENTRYPOINTS_H */
