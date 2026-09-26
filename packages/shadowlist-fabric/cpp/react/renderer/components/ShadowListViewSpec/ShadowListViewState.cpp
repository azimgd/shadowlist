#include "ShadowListViewState.h"

#ifdef ANDROID
#include <jni.h>

#include <unordered_map>
#endif

namespace facebook::react {

#ifdef ANDROID

namespace {

std::mutex& liveScrollRegistryMutex() {
  static std::mutex mutex;
  return mutex;
}

/*
 * Weak entries, so the registry never keeps a list alive. Dead entries are swept when a new
 * list registers, and there are only ever a few lists.
 */
std::unordered_map<std::int64_t, std::weak_ptr<ShadowListLiveScroll>>& liveScrollRegistry() {
  static std::unordered_map<std::int64_t, std::weak_ptr<ShadowListLiveScroll>> registry;
  return registry;
}

}

void registerShadowListLiveScroll(const std::shared_ptr<ShadowListLiveScroll>& liveScroll) {
  if (!liveScroll) {
    return;
  }
  std::lock_guard<std::mutex> lock(liveScrollRegistryMutex());
  auto& registry = liveScrollRegistry();
  if (registry.find(liveScroll->handle()) != registry.end()) {
    return;
  }
  for (auto entry = registry.begin(); entry != registry.end();) {
    entry = entry->second.expired() ? registry.erase(entry) : std::next(entry);
  }
  registry.emplace(liveScroll->handle(), liveScroll);
}

std::shared_ptr<ShadowListLiveScroll> findShadowListLiveScroll(std::int64_t handle) {
  std::lock_guard<std::mutex> lock(liveScrollRegistryMutex());
  auto& registry = liveScrollRegistry();
  auto entry = registry.find(handle);
  return entry == registry.end() ? nullptr : entry->second.lock();
}

#endif

}

#ifdef ANDROID
/*
 * com.shadowlist.ShadowListLiveScroll.nativeWrite. Stores the Java host's scroll report and
 * returns its sequence, or 0 when the list is gone, which tells the host to send a full
 * state update instead.
 */
extern "C" JNIEXPORT jlong JNICALL Java_com_shadowlist_ShadowListLiveScroll_nativeWrite(
  JNIEnv* /*env*/,
  jclass /*clazz*/,
  jlong handle,
  jdouble offsetX,
  jdouble offsetY,
  jboolean userScrolled,
  jdouble scrollPhase,
  jdouble commitToken,
  jdouble concealGenerationAck) {
  auto liveScroll = facebook::react::findShadowListLiveScroll(static_cast<std::int64_t>(handle));
  if (!liveScroll) {
    return 0;
  }
  facebook::react::ShadowListLiveScroll::Report report;
  report.offsetX = offsetX;
  report.offsetY = offsetY;
  report.userScrolled = userScrolled == JNI_TRUE;
  report.scrollPhase = scrollPhase;
  report.commitToken = commitToken;
  report.concealGenerationAck = concealGenerationAck;
  return static_cast<jlong>(liveScroll->write(report));
}
#endif
