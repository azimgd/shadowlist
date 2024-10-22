#include <jni.h>
#include <jsi/jsi.h>
#include <fbjni/fbjni.h>
#include <ReactCommon/CallInvokerHolder.h>
#include <typeinfo>
#include "SLComponentRegistry.h"

extern "C"
JNIEXPORT jlong JNICALL Java_com_shadowlist_SLComponentRegistry_nativeInit(JNIEnv *env, jobject thiz, jint initialNumToRender) {
  auto *registry = new SLComponentRegistry(initialNumToRender);
  return reinterpret_cast<jlong>(registry);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeRegisterComponent(JNIEnv *env, jobject thiz, jlong registryPtr, jint componentId) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  registry->registerComponent(componentId);
}


extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeUnregisterComponent(JNIEnv *env, jobject thiz, jlong registryPtr, jint componentId) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  registry->unregisterComponent(componentId);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeMountRange(JNIEnv *env, jobject thiz, jlong registryPtr, jint visibleStartIndex, jint visibleEndIndex) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  registry->mountRange(visibleStartIndex, visibleEndIndex);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeMount(JNIEnv *env, jobject thiz, jlong registryPtr, jintArray indices) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  jint *elements = env->GetIntArrayElements(indices, nullptr);
  jsize length = env->GetArrayLength(indices);

  std::vector<int> indexVector(elements, elements + length);
  registry->mount(indexVector);

  env->ReleaseIntArrayElements(indices, elements, 0);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeUnmount(JNIEnv *env, jobject thiz, jlong registryPtr, jintArray indices) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  jint *elements = env->GetIntArrayElements(indices, nullptr);
  jsize length = env->GetArrayLength(indices);

  std::vector<int> indexVector(elements, elements + length);
  registry->unmount(indexVector);

  env->ReleaseIntArrayElements(indices, elements, 0);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeMountObserver(JNIEnv *env, jobject thiz, jlong registryPtr, jobject observer) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);

  auto observerCallback = [env, observer](int id, bool isVisible) {
    jclass observerClass = env->GetObjectClass(observer);
    jmethodID methodId = env->GetMethodID(observerClass, "onVisibilityChanged", "(IZ)V");
    if (methodId) {
      env->CallVoidMethod(observer, methodId, id, static_cast<jboolean>(isVisible));
    }
  };

  registry->mountObserver(observerCallback);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeUnmountObserver(JNIEnv *env, jobject thiz, jlong registryPtr, jobject observer) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);

  auto observerCallback = [env, observer](int id, bool isVisible) {
    jclass observerClass = env->GetObjectClass(observer);
    jmethodID methodId = env->GetMethodID(observerClass, "onVisibilityChanged", "(IZ)V");
    if (methodId) {
      env->CallVoidMethod(observer, methodId, id, static_cast<jboolean>(isVisible));
    }
  };

  registry->unmountObserver(observerCallback);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLComponentRegistry_nativeDestroy(JNIEnv *env, jobject thiz, jlong registryPtr) {
  auto *registry = reinterpret_cast<SLComponentRegistry*>(registryPtr);
  if (registry != nullptr) {
    delete registry;
  }
}
