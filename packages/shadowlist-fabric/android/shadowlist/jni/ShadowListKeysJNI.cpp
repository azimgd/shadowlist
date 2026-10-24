/*
 * JNI side of com.shadowlist.ShadowListKeys, and the ShadowListStore TurboModule's registration,
 * which runs when this library loads, before JS can ask for the module.
 */

#include <jni.h>

#include <cstdint>
#include <string>

#include "ShadowListStoreJsi.h"
#include "ShadowListStoreModule.h"

#define SL_KEYS_JNI(name) Java_com_shadowlist_ShadowListKeys_##name

namespace {

const bool storeModuleRegistered = [] {
  facebook::react::registerShadowListStoreModule();
  return true;
}();

std::shared_ptr<const azimgd::shadowlist::RowKeysSnapshot> snapshotOf(jlong store, jdouble version) {
  if (store <= 0 || version <= 0) {
    return nullptr;
  }
  auto keys = facebook::react::shadowlist::findRowKeyStore(static_cast<std::int64_t>(store));
  return keys ? keys->snapshot(static_cast<std::uint64_t>(version)) : nullptr;
}

}

extern "C" {

JNIEXPORT jint JNICALL SL_KEYS_JNI(nativeKeyCount)(JNIEnv*, jclass, jlong store, jdouble version) {
  auto snapshot = snapshotOf(store, version);
  return snapshot ? static_cast<jint>(snapshot->size()) : -1;
}

JNIEXPORT jint JNICALL SL_KEYS_JNI(nativeIndexOfKey)(JNIEnv* env, jclass, jlong store, jdouble version, jstring key) {
  auto snapshot = snapshotOf(store, version);
  if (!snapshot || key == nullptr) {
    return -1;
  }
  const char* chars = env->GetStringUTFChars(key, nullptr);
  std::string value = chars != nullptr ? chars : "";
  if (chars != nullptr) {
    env->ReleaseStringUTFChars(key, chars);
  }
  std::size_t row = snapshot->indexOfKey(value);
  return row == azimgd::shadowlist::UNDEFINED_INDEX ? -1 : static_cast<jint>(row);
}

}
