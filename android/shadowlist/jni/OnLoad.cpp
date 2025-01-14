#include <fbjni/fbjni.h>

#include "ShadowlistModule.h"
#include "SLRuntimeManager.h"
#include "SLModuleJSI.h"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
  return facebook::jni::initialize(vm, [] {
    facebook::react::ShadowlistModule::registerNatives();
  });
}

extern "C" JNIEXPORT void JNICALL Java_com_shadowlist_ShadowlistModule_injectJSIBindings(JNIEnv *env, jobject thiz, jobject jsiRuntime) {
  jsi::Runtime* runtime = reinterpret_cast<jsi::Runtime*>(jsiRuntime);
  SLRuntimeManager::getInstance().setRuntime(runtime);
  SLModuleJSI::install(*runtime);
}
