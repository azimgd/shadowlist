/*
 * JNI side of com.shadowlist.ShadowListScrollSync. Each Java view owns one ScrollSync from the
 * host layer, reached by pointer, and reads results from an array it reuses, so scroll frames
 * allocate nothing. Only the UI thread calls in.
 */

#include <jni.h>

#include <shadowlist-core/host/LiveScroll.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>

#include <cstdint>

namespace {

namespace sl = azimgd::shadowlist;

// Slots of ShadowListScrollSync.OUT_*.
enum OutSlot {
  OUT_COMMIT = 0,
  OUT_FRAME_USER_SCROLLED,
  OUT_OFFSET_X,
  OUT_OFFSET_Y,
  OUT_OFFSET_ENABLED,
  OUT_USER_SCROLLED,
  OUT_SCROLL_PHASE,
  OUT_COMMIT_TOKEN,
  OUT_SEQUENCE,
  OUT_HAS_COMMAND,
  OUT_COMMAND_INDEX,
  OUT_COMMAND_SEQUENCE,
  OUT_COMMAND_VIEW_POSITION,
  OUT_ACTION_KIND,
  OUT_ACTION_X,
  OUT_ACTION_Y,
  OUT_ACTION_TOKEN,
  OUT_ACTION_SHIFTED,
  OUT_ACTION_PRESERVE_MOMENTUM,
  OUT_SLOTS,
};

/*
 * A view's ScrollSync, the live report it writes to, and the last correction it was told about.
 */
struct Host {
  sl::ScrollSync sync;
  std::int64_t liveHandle = 0;
  std::shared_ptr<sl::LiveScroll> liveScroll;
  sl::MountAction action;
};

Host* host(jlong pointer) {
  return reinterpret_cast<Host*>(static_cast<std::intptr_t>(pointer));
}

void writePatch(JNIEnv* env, jdoubleArray out, const sl::ScrollPatch& patch, bool commit, bool frameUserScrolled) {
  jdouble slots[OUT_ACTION_KIND];
  slots[OUT_COMMIT] = commit ? 1.0 : 0.0;
  slots[OUT_FRAME_USER_SCROLLED] = frameUserScrolled ? 1.0 : 0.0;
  slots[OUT_OFFSET_X] = patch.report.offsetX;
  slots[OUT_OFFSET_Y] = patch.report.offsetY;
  slots[OUT_OFFSET_ENABLED] = patch.offsetEnabled ? 1.0 : 0.0;
  slots[OUT_USER_SCROLLED] = patch.report.userScrolled ? 1.0 : 0.0;
  slots[OUT_SCROLL_PHASE] = patch.report.scrollPhase;
  slots[OUT_COMMIT_TOKEN] = patch.report.commitToken;
  slots[OUT_SEQUENCE] = static_cast<double>(patch.report.sequence);
  slots[OUT_HAS_COMMAND] = patch.hasCommand ? 1.0 : 0.0;
  slots[OUT_COMMAND_INDEX] = patch.commandIndex;
  slots[OUT_COMMAND_SEQUENCE] = patch.commandSequence;
  slots[OUT_COMMAND_VIEW_POSITION] = patch.commandViewPosition;
  env->SetDoubleArrayRegion(out, 0, OUT_ACTION_KIND, slots);
}

}

extern "C" JNIEXPORT jlong JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeCreate(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jdouble landingTolerance) {
  sl::ScrollSync::Options options;
  options.landingTolerance = landingTolerance;
  options.exactEcho = true;
  auto* created = new Host();
  created->sync = sl::ScrollSync(options);
  return static_cast<jlong>(reinterpret_cast<std::intptr_t>(created));
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeDestroy(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  delete host(pointer);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeReset(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer,
  jboolean horizontal) {
  Host* target = host(pointer);
  target->sync.reset();
  target->sync.setHorizontal(horizontal == JNI_TRUE);
  target->liveHandle = 0;
  target->liveScroll = nullptr;
  target->action = {};
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeSetHorizontal(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer,
  jboolean horizontal) {
  host(pointer)->sync.setHorizontal(horizontal == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeBeginMount(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer,
  jlong liveHandle,
  jboolean offsetEnabled,
  jdouble offsetX,
  jdouble offsetY,
  jdouble baseX,
  jdouble baseY,
  jdouble commitToken,
  jboolean userScrolled,
  jdouble scrollPhase,
  jdouble concealGeneration,
  jdouble commandSequence,
  jdouble bandLow,
  jdouble bandHigh) {
  Host* target = host(pointer);
  // The registry lookup takes a lock, so only look the list up again when it changed.
  if (liveHandle != target->liveHandle || !target->liveScroll) {
    target->liveHandle = liveHandle;
    target->liveScroll = liveHandle != 0 ? sl::LiveScroll::find(liveHandle) : nullptr;
  }
  sl::MountedScroll mounted;
  mounted.offsetEnabled = offsetEnabled == JNI_TRUE;
  mounted.offsetX = offsetX;
  mounted.offsetY = offsetY;
  mounted.baseX = baseX;
  mounted.baseY = baseY;
  mounted.commitToken = static_cast<std::uint64_t>(commitToken);
  mounted.userScrolled = userScrolled == JNI_TRUE;
  mounted.scrollPhase = scrollPhase;
  mounted.concealGeneration = concealGeneration;
  mounted.commandSequence = commandSequence;
  mounted.band.low = bandLow;
  mounted.band.high = bandHigh;
  target->sync.beginMount(mounted, target->liveScroll);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeSetApplyingContentSize(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer,
  jboolean applying) {
  host(pointer)->sync.setApplyingContentSize(applying == JNI_TRUE);
}

extern "C" JNIEXPORT jint JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeCorrection(
  JNIEnv* env,
  jclass /*clazz*/,
  jlong pointer,
  jdouble offsetX,
  jdouble offsetY,
  jdouble minOffset,
  jdouble maxOffset,
  jboolean touching,
  jboolean moving,
  jboolean ownsOffset,
  jdoubleArray out) {
  Host* target = host(pointer);
  sl::ViewMotion motion;
  motion.offsetX = offsetX;
  motion.offsetY = offsetY;
  motion.shiftFromX = offsetX;
  motion.shiftFromY = offsetY;
  motion.minOffset = minOffset;
  motion.maxOffset = maxOffset;
  motion.touching = touching == JNI_TRUE;
  motion.moving = moving == JNI_TRUE;
  motion.ownsOffset = ownsOffset == JNI_TRUE;
  target->action = target->sync.correction(motion);
  const auto& action = target->action;
  jdouble slots[OUT_SLOTS - OUT_ACTION_KIND];
  slots[0] = static_cast<double>(action.kind);
  slots[1] = action.offsetX;
  slots[2] = action.offsetY;
  slots[3] = static_cast<double>(action.token);
  slots[4] = action.shifted ? 1.0 : 0.0;
  slots[5] = action.preserveMomentum ? 1.0 : 0.0;
  env->SetDoubleArrayRegion(out, OUT_ACTION_KIND, OUT_SLOTS - OUT_ACTION_KIND, slots);
  return static_cast<jint>(action.kind);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeWillWrite(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  Host* target = host(pointer);
  target->sync.willWrite(target->action);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeDidWrite(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer,
  jboolean moved) {
  host(pointer)->sync.didWrite(moved == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeEndMount(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  host(pointer)->sync.endMount();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeOnScroll(
  JNIEnv* env,
  jclass /*clazz*/,
  jlong pointer,
  jdouble offsetX,
  jdouble offsetY,
  jdouble scrollPhase,
  jboolean commitEveryFrame,
  jdoubleArray out) {
  sl::ScrollFrame frame;
  frame.offsetX = offsetX;
  frame.offsetY = offsetY;
  frame.scrollPhase = scrollPhase;
  frame.commitEveryFrame = commitEveryFrame == JNI_TRUE;
  auto report = host(pointer)->sync.onScroll(frame);
  if (report.needsCommit) {
    writePatch(env, out, report.patch, true, report.userScrolled);
  } else {
    jdouble slots[2] = {0.0, report.userScrolled ? 1.0 : 0.0};
    env->SetDoubleArrayRegion(out, 0, 2, slots);
  }
  return report.needsCommit ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeArm(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer,
  jdouble offsetX,
  jdouble offsetY,
  jboolean animated) {
  host(pointer)->sync.arm(offsetX, offsetY, animated == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeDisarm(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  host(pointer)->sync.disarm();
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeMomentumStopped(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  host(pointer)->sync.momentumStopped();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeArmedAnimated(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  return host(pointer)->sync.armedAnimated() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeLivePatch(
  JNIEnv* env,
  jclass /*clazz*/,
  jlong pointer,
  jdouble offsetX,
  jdouble offsetY,
  jboolean current,
  jboolean userScrolled,
  jdouble scrollPhase,
  jdoubleArray out) {
  auto& sync = host(pointer)->sync;
  auto patch = current == JNI_TRUE ? sync.livePatch(offsetX, offsetY)
                                   : sync.livePatch(offsetX, offsetY, userScrolled == JNI_TRUE, scrollPhase);
  writePatch(env, out, patch, true, false);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeClearUserScrolled(
  JNIEnv* env,
  jclass /*clazz*/,
  jlong pointer,
  jdouble offsetX,
  jdouble offsetY,
  jdoubleArray out) {
  auto patch = host(pointer)->sync.clearUserScrolled(offsetX, offsetY);
  if (!patch) {
    return JNI_FALSE;
  }
  writePatch(env, out, *patch, true, false);
  return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeIssueCommand(
  JNIEnv* env,
  jclass /*clazz*/,
  jlong pointer,
  jdouble index,
  jdouble viewPosition,
  jdouble offsetX,
  jdouble offsetY,
  jboolean momentumYielded,
  jdoubleArray out) {
  auto patch = host(pointer)->sync.issueCommand(index, viewPosition, offsetX, offsetY, momentumYielded == JNI_TRUE);
  writePatch(env, out, patch, true, false);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeCurrentUserScrolled(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  return host(pointer)->sync.currentUserScrolled() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jdouble JNICALL Java_com_shadowlist_ShadowListScrollSync_nativeCurrentScrollPhase(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong pointer) {
  return host(pointer)->sync.currentScrollPhase();
}
