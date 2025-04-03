#include <fbjni/fbjni.h>

#include "ShadowlistModule.h"

using namespace azimgd::shadowlist;
using namespace facebook::react;
using namespace facebook::jni;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
  return initialize(vm, [] {
    ShadowlistModule::registerNatives();
  });
}
