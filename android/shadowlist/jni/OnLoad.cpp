#include <fbjni/fbjni.h>

#include "ShadowlistModule.h"
#include "SLRuntimeManager.h"
#include "SLModuleJSI.h"

using namespace azimgd::shadowlist;
using namespace facebook::react;
using namespace facebook::jni;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
  return initialize(vm, [] {
    ShadowlistModule::registerNatives();
  });
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowlistModule_injectJSIBindings(JNIEnv *env, jobject thiz, jobject jsiRuntime) {
  jsi::Runtime* runtime = reinterpret_cast<jsi::Runtime*>(jsiRuntime);
  SLRuntimeManager::getInstance().setRuntime(runtime);
  SLModuleJSI::install(*runtime);
}
