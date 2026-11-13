#include <jni.h>

#include <shadowlist-core/host/TreeSource.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace sl = azimgd::shadowlist;

#define SHADOWLIST_TREE_JNI(name) Java_com_shadowlist_kit_ShadowListKitTreeRows_##name

namespace {

sl::TreeSource &sourceOf(jlong handle) { return *reinterpret_cast<sl::TreeSource *>(handle); }

std::vector<std::string> stringsOf(JNIEnv *env, jobjectArray values) {
  std::vector<std::string> result;
  auto count = env->GetArrayLength(values);
  result.reserve(count);
  for (jsize index = 0; index < count; ++index) {
    auto value = static_cast<jstring>(env->GetObjectArrayElement(values, index));
    const char *text = env->GetStringUTFChars(value, nullptr);
    result.emplace_back(text);
    env->ReleaseStringUTFChars(value, text);
    env->DeleteLocalRef(value);
  }
  return result;
}

jobjectArray stringsTo(JNIEnv *env, const std::vector<std::string> &values) {
  auto stringClass = env->FindClass("java/lang/String");
  auto result = env->NewObjectArray(static_cast<jsize>(values.size()), stringClass, nullptr);
  env->DeleteLocalRef(stringClass);
  for (std::size_t index = 0; index < values.size(); ++index) {
    auto value = env->NewStringUTF(values[index].c_str());
    env->SetObjectArrayElement(result, static_cast<jsize>(index), value);
    env->DeleteLocalRef(value);
  }
  return result;
}

enum RowSlot : int { ROW_DEPTH = 0, ROW_FLAGS, ROW_SLOTS };

}

extern "C" {

JNIEXPORT jlong JNICALL SHADOWLIST_TREE_JNI(nativeCreate)(JNIEnv *, jobject) {
  return reinterpret_cast<jlong>(new sl::TreeSource());
}

JNIEXPORT void JNICALL SHADOWLIST_TREE_JNI(nativeDestroy)(JNIEnv *, jclass, jlong handle) {
  delete reinterpret_cast<sl::TreeSource *>(handle);
}

JNIEXPORT void JNICALL SHADOWLIST_TREE_JNI(nativeReset)(JNIEnv *env, jobject, jlong handle, jobjectArray keys, jintArray depths, jintArray flags) {
  auto names = stringsOf(env, keys);
  std::vector<jint> rowDepths(names.size()), rowFlags(names.size());
  env->GetIntArrayRegion(depths, 0, static_cast<jsize>(names.size()), rowDepths.data());
  env->GetIntArrayRegion(flags, 0, static_cast<jsize>(names.size()), rowFlags.data());
  std::vector<sl::TreeRow> rows;
  rows.reserve(names.size());
  for (std::size_t index = 0; index < names.size(); ++index) {
    rows.push_back({std::move(names[index]), static_cast<std::uint32_t>(rowDepths[index]),
      (rowFlags[index] & sl::TREE_ROW_HAS_CHILDREN) != 0, (rowFlags[index] & sl::TREE_ROW_EXPANDED) != 0});
  }
  sourceOf(handle).reset(std::move(rows));
}

JNIEXPORT void JNICALL SHADOWLIST_TREE_JNI(nativeSetExpandedKeys)(JNIEnv *env, jobject, jlong handle, jobjectArray keys) {
  sourceOf(handle).setExpandedKeys(stringsOf(env, keys));
}

JNIEXPORT void JNICALL SHADOWLIST_TREE_JNI(nativeToggle)(JNIEnv *env, jobject, jlong handle, jstring key) {
  const char *text = env->GetStringUTFChars(key, nullptr);
  sourceOf(handle).toggle(std::string(text));
  env->ReleaseStringUTFChars(key, text);
}

JNIEXPORT void JNICALL SHADOWLIST_TREE_JNI(nativeExpand)(JNIEnv *, jobject, jlong handle, jint index) { sourceOf(handle).expand(index); }
JNIEXPORT void JNICALL SHADOWLIST_TREE_JNI(nativeCollapse)(JNIEnv *, jobject, jlong handle, jint index) { sourceOf(handle).collapse(index); }

JNIEXPORT jobjectArray JNICALL SHADOWLIST_TREE_JNI(nativeKeys)(JNIEnv *env, jobject, jlong handle) {
  auto &rows = sourceOf(handle).rows();
  std::vector<std::string> keys;
  keys.reserve(rows.size());
  for (std::size_t index = 0; index < rows.size(); ++index) keys.push_back(rows.rowAt(index).key);
  return stringsTo(env, keys);
}

JNIEXPORT jintArray JNICALL SHADOWLIST_TREE_JNI(nativeMetadata)(JNIEnv *env, jobject, jlong handle) {
  auto &rows = sourceOf(handle).rows();
  std::vector<jint> values(rows.size() * ROW_SLOTS);
  for (std::size_t index = 0; index < rows.size(); ++index) {
    auto row = rows.rowAt(index);
    values[index * ROW_SLOTS + ROW_DEPTH] = static_cast<jint>(row.depth);
    values[index * ROW_SLOTS + ROW_FLAGS] = (row.hasChildren ? sl::TREE_ROW_HAS_CHILDREN : 0) | (row.expanded ? sl::TREE_ROW_EXPANDED : 0);
  }
  auto result = env->NewIntArray(static_cast<jsize>(values.size()));
  env->SetIntArrayRegion(result, 0, static_cast<jsize>(values.size()), values.data());
  return result;
}

JNIEXPORT jobjectArray JNICALL SHADOWLIST_TREE_JNI(nativeExpandedKeys)(JNIEnv *env, jobject, jlong handle) {
  return stringsTo(env, sourceOf(handle).expandedKeys());
}

}
