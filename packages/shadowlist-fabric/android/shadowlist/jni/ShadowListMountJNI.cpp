/*
 * JNI side of com.shadowlist.ShadowListMountTicker. The Java view reaches its list's
 * ShadowListMountDriver by the handle in the state's MapBuffer and ticks it on Choreographer
 * frames. Only the UI thread calls in.
 */

#include <jni.h>

#include <cstdint>

#include "ShadowListMountDriver.h"

#define SL_MOUNT_JNI(name) Java_com_shadowlist_ShadowListMountTicker_##name

extern "C" {

JNIEXPORT jboolean JNICALL SL_MOUNT_JNI(nativeNeedsTick)(JNIEnv*, jclass, jlong handle) {
  auto driver = facebook::react::ShadowListMountDriver::find(static_cast<std::int64_t>(handle));
  return driver && driver->needsTick() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL SL_MOUNT_JNI(nativeTick)(JNIEnv*, jclass, jlong handle) {
  auto driver = facebook::react::ShadowListMountDriver::find(static_cast<std::int64_t>(handle));
  return driver && driver->tick() ? JNI_TRUE : JNI_FALSE;
}

}
