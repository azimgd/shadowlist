/*
 * JNI side of com.shadowlist.ShadowListScrollSync. Each Java view owns one ScrollSync from the
 * host layer, reached by handle, and reads results from an array it reuses. Scroll frames
 * allocate nothing. Only the UI thread calls in.
 */

#include <jni.h>

#include <cstdint>
#include <memory>

#include <shadowlist-core/host/LiveScroll.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>

namespace {

namespace sl = azimgd::shadowlist;

/*
 * Slots of ShadowListScrollSync.OUT_*.
 */
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
  OUT_COMMAND_ROW_OFFSET,
  OUT_COMMAND_ANIMATED,
  OUT_HAS_ANCHOR_REQUEST,
  OUT_ANCHOR_REQUEST_SEQUENCE,
  OUT_FRAME_LANDED,
  OUT_SLOTS,
};

/*
 * A view's ScrollSync, the live report it writes to, and the last correction it was told about.
 */
struct Peer {
  sl::ScrollSync sync;
  std::int64_t liveHandle = 0;
  std::shared_ptr<sl::LiveScroll> liveScroll;
  sl::MountAction action;
};

Peer* peerOf(jlong handle) {
  return reinterpret_cast<Peer*>(handle);
}

void writePatch(
  JNIEnv* env,
  jdoubleArray out,
  const sl::ScrollPatch& patch,
  bool commit,
  bool frameUserScrolled,
  bool frameLanded = false) {
  jdouble slots[OUT_ACTION_KIND];
  slots[OUT_COMMIT] = commit ? 1.0 : 0.0;
  slots[OUT_FRAME_USER_SCROLLED] = frameUserScrolled ? 1.0 : 0.0;
  slots[OUT_OFFSET_X] = patch.report.offsetX;
  slots[OUT_OFFSET_Y] = patch.report.offsetY;
  slots[OUT_OFFSET_ENABLED] = patch.offsetEnabled ? 1.0 : 0.0;
  slots[OUT_USER_SCROLLED] = patch.report.userScrolled ? 1.0 : 0.0;
  slots[OUT_SCROLL_PHASE] = patch.report.scrollPhase;
  slots[OUT_COMMIT_TOKEN] = static_cast<double>(patch.report.commitToken);
  slots[OUT_SEQUENCE] = static_cast<double>(patch.report.sequence);
  slots[OUT_HAS_COMMAND] = patch.hasCommand ? 1.0 : 0.0;
  slots[OUT_COMMAND_INDEX] = patch.commandIndex;
  slots[OUT_COMMAND_SEQUENCE] = patch.commandSequence;
  slots[OUT_COMMAND_VIEW_POSITION] = patch.commandViewPosition;
  env->SetDoubleArrayRegion(out, 0, OUT_ACTION_KIND, slots);
  jdouble more[OUT_SLOTS - OUT_COMMAND_ROW_OFFSET];
  more[OUT_COMMAND_ROW_OFFSET - OUT_COMMAND_ROW_OFFSET] = patch.commandRowOffset;
  more[OUT_COMMAND_ANIMATED - OUT_COMMAND_ROW_OFFSET] = patch.commandAnimated ? 1.0 : 0.0;
  more[OUT_HAS_ANCHOR_REQUEST - OUT_COMMAND_ROW_OFFSET] = patch.hasAnchorRequest ? 1.0 : 0.0;
  more[OUT_ANCHOR_REQUEST_SEQUENCE - OUT_COMMAND_ROW_OFFSET] = patch.anchorRequestSequence;
  more[OUT_FRAME_LANDED - OUT_COMMAND_ROW_OFFSET] = frameLanded ? 1.0 : 0.0;
  env->SetDoubleArrayRegion(out, OUT_COMMAND_ROW_OFFSET, OUT_SLOTS - OUT_COMMAND_ROW_OFFSET, more);
}

}

#define SL_SCROLL_SYNC_JNI(name) Java_com_shadowlist_ShadowListScrollSync_##name

extern "C" {

JNIEXPORT jlong JNICALL SL_SCROLL_SYNC_JNI(nativeCreate)(JNIEnv*, jclass, jdouble landingTolerance) {
  sl::ScrollSync::Options options;
  options.landingTolerance = landingTolerance;
  options.exactEcho = true;
  auto* peer = new Peer();
  peer->sync = sl::ScrollSync(options);
  return reinterpret_cast<jlong>(peer);
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeDestroy)(JNIEnv*, jclass, jlong handle) {
  delete peerOf(handle);
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeReset)(JNIEnv*, jclass, jlong handle, jboolean horizontal) {
  Peer* peer = peerOf(handle);
  peer->sync.reset();
  peer->sync.setHorizontal(horizontal == JNI_TRUE);
  peer->liveHandle = 0;
  peer->liveScroll = nullptr;
  peer->action = {};
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeSetHorizontal)(JNIEnv*, jclass, jlong handle, jboolean horizontal) {
  peerOf(handle)->sync.setHorizontal(horizontal == JNI_TRUE);
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeBeginMount)(
  JNIEnv*,
  jclass,
  jlong handle,
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
  jdouble animationSequence,
  jdouble animationOffset,
  jdouble bandLow,
  jdouble bandHigh) {
  Peer* peer = peerOf(handle);
  // The registry lookup takes a lock. Only look the list up again when it changed.

  if (liveHandle != peer->liveHandle || !peer->liveScroll) {
    peer->liveHandle = liveHandle;
    peer->liveScroll = liveHandle != 0 ? sl::LiveScroll::find(liveHandle) : nullptr;
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
  mounted.animationSequence = animationSequence;
  mounted.animationOffset = animationOffset;
  mounted.band.low = bandLow;
  mounted.band.high = bandHigh;
  peer->sync.beginMount(mounted, peer->liveScroll);
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeSetApplyingContentSize)(
  JNIEnv*,
  jclass,
  jlong handle,
  jboolean applying) {
  peerOf(handle)->sync.setApplyingContentSize(applying == JNI_TRUE);
}

JNIEXPORT jint JNICALL SL_SCROLL_SYNC_JNI(nativeCorrection)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble offsetX,
  jdouble offsetY,
  jdouble minOffset,
  jdouble maxOffset,
  jboolean touching,
  jboolean moving,
  jboolean ownsOffset,
  jdoubleArray out) {
  Peer* peer = peerOf(handle);
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
  peer->action = peer->sync.correction(motion);
  const auto& action = peer->action;
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

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeWillWrite)(JNIEnv*, jclass, jlong handle) {
  Peer* peer = peerOf(handle);
  peer->sync.willWrite(peer->action);
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeDidWrite)(JNIEnv*, jclass, jlong handle, jboolean moved) {
  peerOf(handle)->sync.didWrite(moved == JNI_TRUE);
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeEndMount)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->sync.endMount();
}

JNIEXPORT jboolean JNICALL SL_SCROLL_SYNC_JNI(nativeOnScroll)(
  JNIEnv* env,
  jclass,
  jlong handle,
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
  auto report = peerOf(handle)->sync.onScroll(frame);
  if (report.needsCommit) {
    writePatch(env, out, report.patch, true, report.userScrolled, report.landed);
  } else {
    jdouble slots[2] = {0.0, report.userScrolled ? 1.0 : 0.0};
    env->SetDoubleArrayRegion(out, 0, 2, slots);
    jdouble landed = report.landed ? 1.0 : 0.0;
    env->SetDoubleArrayRegion(out, OUT_FRAME_LANDED, 1, &landed);
  }
  return report.needsCommit ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeArm)(
  JNIEnv*,
  jclass,
  jlong handle,
  jdouble offsetX,
  jdouble offsetY,
  jboolean animated) {
  peerOf(handle)->sync.arm(offsetX, offsetY, animated == JNI_TRUE);
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeDisarm)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->sync.disarm();
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeMomentumStopped)(JNIEnv*, jclass, jlong handle) {
  peerOf(handle)->sync.momentumStopped();
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeLivePatch)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble offsetX,
  jdouble offsetY,
  jboolean current,
  jboolean userScrolled,
  jdouble scrollPhase,
  jdoubleArray out) {
  auto& sync = peerOf(handle)->sync;
  auto patch = current == JNI_TRUE ? sync.livePatch(offsetX, offsetY)
                                   : sync.livePatch(offsetX, offsetY, userScrolled == JNI_TRUE, scrollPhase);
  writePatch(env, out, patch, true, false);
}

JNIEXPORT jboolean JNICALL SL_SCROLL_SYNC_JNI(nativeClearUserScrolled)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble offsetX,
  jdouble offsetY,
  jdoubleArray out) {
  auto patch = peerOf(handle)->sync.clearUserScrolled(offsetX, offsetY);
  if (!patch) {
    return JNI_FALSE;
  }
  writePatch(env, out, *patch, true, false);
  return JNI_TRUE;
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeIssueCommand)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble index,
  jdouble viewPosition,
  jdouble rowOffset,
  jboolean animated,
  jdouble offsetX,
  jdouble offsetY,
  jboolean momentumYielded,
  jdoubleArray out) {
  sl::ScrollCommand command{index, viewPosition, rowOffset, animated == JNI_TRUE};
  auto patch = peerOf(handle)->sync.issueCommand(command, offsetX, offsetY, momentumYielded == JNI_TRUE);
  writePatch(env, out, patch, true, false);
}

JNIEXPORT jboolean JNICALL SL_SCROLL_SYNC_JNI(nativeLand)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble offsetX,
  jdouble offsetY,
  jdoubleArray out) {
  auto patch = peerOf(handle)->sync.land(offsetX, offsetY);
  if (!patch) {
    return JNI_FALSE;
  }
  writePatch(env, out, *patch, true, false);
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL SL_SCROLL_SYNC_JNI(nativeIsLanding)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->sync.isLanding() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SL_SCROLL_SYNC_JNI(nativeRequestAnchor)(
  JNIEnv* env,
  jclass,
  jlong handle,
  jdouble offsetX,
  jdouble offsetY,
  jdoubleArray out) {
  writePatch(env, out, peerOf(handle)->sync.requestAnchor(offsetX, offsetY), true, false);
}

JNIEXPORT jboolean JNICALL SL_SCROLL_SYNC_JNI(nativeCurrentUserScrolled)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->sync.isCurrentUserScrolled() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL SL_SCROLL_SYNC_JNI(nativeCurrentScrollPhase)(JNIEnv*, jclass, jlong handle) {
  return peerOf(handle)->sync.getCurrentScrollPhase();
}

}
