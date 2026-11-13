#include <jni.h>

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/host/DragBoard.hpp>

#include <string>
#include <vector>

namespace sl = azimgd::shadowlist;

namespace {
std::string stringOf(JNIEnv *env, jstring value) {
  const char *text = env->GetStringUTFChars(value, nullptr);
  std::string result(text);
  env->ReleaseStringUTFChars(value, text);
  return result;
}

enum PlanSlot : int { PLAN_SOURCE_COLUMN = 0, PLAN_SOURCE_INDEX, PLAN_DESTINATION_COLUMN, PLAN_DESTINATION_INDEX, PLAN_SLOTS };
}

extern "C" JNIEXPORT jintArray JNICALL Java_com_shadowlist_kit_ShadowListKitBoardMovePlan_nativePlan(
  JNIEnv *env, jclass, jobjectArray columns, jstring itemKey, jint destinationColumn, jstring destinationKey, jint destinationIndex) {
  std::vector<std::vector<std::string>> encoded;
  auto count = env->GetArrayLength(columns);
  encoded.reserve(count);
  for (jsize column = 0; column < count; ++column) {
    auto keys = static_cast<jobjectArray>(env->GetObjectArrayElement(columns, column));
    std::vector<std::string> names;
    auto keyCount = env->GetArrayLength(keys);
    names.reserve(keyCount);
    for (jsize index = 0; index < keyCount; ++index) {
      auto key = static_cast<jstring>(env->GetObjectArrayElement(keys, index));
      names.push_back(stringOf(env, key));
      env->DeleteLocalRef(key);
    }
    encoded.push_back(std::move(names));
    env->DeleteLocalRef(keys);
  }
  auto plan = sl::planBoardMove(encoded, stringOf(env, itemKey), destinationColumn < 0 ? sl::UNDEFINED_INDEX : destinationColumn,
    stringOf(env, destinationKey), destinationIndex < 0 ? sl::UNDEFINED_INDEX : destinationIndex);
  auto indexOf = [](std::size_t index) { return index == sl::UNDEFINED_INDEX ? -1 : static_cast<jint>(index); };
  jint values[PLAN_SLOTS];
  values[PLAN_SOURCE_COLUMN] = indexOf(plan.sourceColumn);
  values[PLAN_SOURCE_INDEX] = indexOf(plan.sourceIndex);
  values[PLAN_DESTINATION_COLUMN] = indexOf(plan.destinationColumn);
  values[PLAN_DESTINATION_INDEX] = indexOf(plan.destinationIndex);
  auto result = env->NewIntArray(PLAN_SLOTS);
  env->SetIntArrayRegion(result, 0, PLAN_SLOTS, values);
  return result;
}
