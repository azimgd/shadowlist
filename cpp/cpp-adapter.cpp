#ifdef ANDROID
#include <jni.h>
#include <fbjni/fbjni.h>
#include <string>
#include "SLFenwickTree.hpp"

/**
 * SLFenwickTree
 */
extern "C"
JNIEXPORT jlong JNICALL Java_com_shadowlist_SLFenwickTree_nativeInit(JNIEnv *env, jobject thiz, jfloatArray childrenMeasurements) {
  if (childrenMeasurements == nullptr) {
    return 0;
  }

  jsize size = env->GetArrayLength(childrenMeasurements);
  facebook::react::SLFenwickTree* childrenMeasurementsTreePtr = new facebook::react::SLFenwickTree(size);

  jfloat* elements = env->GetFloatArrayElements(childrenMeasurements, nullptr);
  if (elements == nullptr) {
    delete childrenMeasurementsTreePtr;
    return 0;
  }

  for (jsize i = 0; i < size; i++) {
    (*childrenMeasurementsTreePtr)[i] = elements[i];
  }

  env->ReleaseFloatArrayElements(childrenMeasurements, elements, 0);
  return reinterpret_cast<jlong>(childrenMeasurementsTreePtr);
}

extern "C"
JNIEXPORT void JNICALL Java_com_shadowlist_SLFenwickTree_nativeDestroy(JNIEnv *env, jobject thiz, jlong treePtr) {
  auto *registry = reinterpret_cast<facebook::react::SLFenwickTree*>(treePtr);
  if (registry != nullptr) {
    delete registry;
  }
}

extern "C"
JNIEXPORT jfloat JNICALL Java_com_shadowlist_SLFenwickTree_nativeSum(JNIEnv *env, jobject thiz, jlong treePtr, jint index) {
  facebook::react::SLFenwickTree* childrenMeasurementsTreePtr = reinterpret_cast<facebook::react::SLFenwickTree*>(treePtr);
  return childrenMeasurementsTreePtr->sum(index);
}

extern "C"
JNIEXPORT jint JNICALL Java_com_shadowlist_SLFenwickTree_nativeLowerBound(JNIEnv *env, jobject thiz, jlong treePtr, jfloat offset) {
  facebook::react::SLFenwickTree* childrenMeasurementsTreePtr = reinterpret_cast<facebook::react::SLFenwickTree*>(treePtr);
  return childrenMeasurementsTreePtr->lower_bound(offset);
}

#endif
