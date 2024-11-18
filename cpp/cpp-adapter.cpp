#ifdef ANDROID
#include <jni.h>
#include <fbjni/fbjni.h>
#include <string>
#include "SLComponentRegistry.h"

extern "C"
JNIEXPORT jlong JNICALL Java_com_shadowlist_SLComponentRegistry_nativeInit(JNIEnv *env, jobject thiz) {
  auto *registry = new SLComponentRegistry();
  return reinterpret_cast<jlong>(registry);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeRegisterComponent(JNIEnv *env, jobject thiz, jlong registryPtr, jstring uniqueId) {
  const char *uniqueIdChars = env->GetStringUTFChars(uniqueId, nullptr);
  std::string uniqueIdStr(uniqueIdChars);
  env->ReleaseStringUTFChars(uniqueId, uniqueIdChars);
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  registry->registerComponent(uniqueIdStr);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeUnregisterComponent(JNIEnv *env, jobject thiz, jlong registryPtr, jstring uniqueId) {
  const char *uniqueIdChars = env->GetStringUTFChars(uniqueId, nullptr);
  std::string uniqueIdStr(uniqueIdChars);
  env->ReleaseStringUTFChars(uniqueId, uniqueIdChars);
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  registry->unregisterComponent(uniqueIdStr);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeMount(JNIEnv *env, jobject thiz, jlong registryPtr, jobjectArray uniqueIds) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  jsize length = env->GetArrayLength(uniqueIds);

  std::vector<std::string> indexVector;
  for (jsize i = 0; i < length; ++i) {
    jstring uniqueId = (jstring) env->GetObjectArrayElement(uniqueIds, i);
    const char *uniqueIdChars = env->GetStringUTFChars(uniqueId, nullptr);
    indexVector.push_back(std::string(uniqueIdChars));
    env->ReleaseStringUTFChars(uniqueId, uniqueIdChars);
  }

  registry->mount(indexVector);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeUnmount(JNIEnv *env, jobject thiz, jlong registryPtr, jobjectArray uniqueIds) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  jsize length = env->GetArrayLength(uniqueIds);

  std::vector<std::string> indexVector;
  for (jsize i = 0; i < length; ++i) {
    jstring uniqueId = (jstring) env->GetObjectArrayElement(uniqueIds, i);
    const char *uniqueIdChars = env->GetStringUTFChars(uniqueId, nullptr);
    indexVector.push_back(std::string(uniqueIdChars));
    env->ReleaseStringUTFChars(uniqueId, uniqueIdChars);
  }

  registry->unmount(indexVector);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeMountObserver(JNIEnv *env, jobject thiz, jlong registryPtr, jobject observer) {
//  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
//
//  jobject globalObserver = env->NewGlobalRef(observer);
//  auto observerCallback = [env, globalObserver](int index, bool isVisible) {
//    jclass observerClass = env->GetObjectClass(globalObserver);
//    jmethodID methodId = env->GetMethodID(observerClass, "onVisibilityChanged", "(IZ)V");
//    env->CallVoidMethod(globalObserver, methodId, index, static_cast<jboolean>(isVisible));
//  };
//
//  registry->mountObserver(observerCallback);
//  // env->DeleteGlobalRef(globalObserver);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeUnmountObserver(JNIEnv *env, jobject thiz, jlong registryPtr, jobject observer) {
//  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
//
//  jobject globalObserver = env->NewGlobalRef(observer);
//  auto observerCallback = [env, globalObserver](int index, bool isVisible) {
//    jclass observerClass = env->GetObjectClass(globalObserver);
//    jmethodID methodId = env->GetMethodID(observerClass, "onVisibilityChanged", "(IZ)V");
//    env->CallVoidMethod(globalObserver, methodId, index, static_cast<jboolean>(isVisible));
//  };
//
//  registry->unmountObserver(observerCallback);
//  // env->DeleteGlobalRef(globalObserver);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeDestroy(JNIEnv *env, jobject thiz, jlong registryPtr) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  if (registry != nullptr) {
    delete registry;
  }
}
#endif
