#include <shadowlist-core/host/LiveScroll.hpp>

#include <unordered_map>

namespace azimgd::shadowlist {

namespace {

bool sameOwner(const std::weak_ptr<const void>& seen, const std::shared_ptr<const void>& published) {
  return !seen.owner_before(published) && !published.owner_before(seen);
}

std::mutex& registryMutex() {
  static std::mutex mutex;
  return mutex;
}

/*
 * Weak entries. The registry never keeps a list alive. Dead entries are swept when a new
 * list registers, and there are only ever a few lists.
 */
std::unordered_map<std::int64_t, std::weak_ptr<LiveScroll>>& registry() {
  static std::unordered_map<std::int64_t, std::weak_ptr<LiveScroll>> entries;
  return entries;
}

}

std::pair<std::uint64_t, std::uint64_t> LiveScroll::geometryVersions(
  const std::shared_ptr<const void>& stickyIndices,
  const std::shared_ptr<const void>& stickyOffsets,
  const std::shared_ptr<const void>& stickySizes,
  const std::shared_ptr<const void>& snapOffsets) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!sameOwner(seenStickyIndices_, stickyIndices) || !sameOwner(seenStickyOffsets_, stickyOffsets) ||
      !sameOwner(seenStickySizes_, stickySizes)) {
    seenStickyIndices_ = stickyIndices;
    seenStickyOffsets_ = stickyOffsets;
    seenStickySizes_ = stickySizes;
    ++stickyVersion_;
  }
  if (!sameOwner(seenSnapOffsets_, snapOffsets)) {
    seenSnapOffsets_ = snapOffsets;
    ++snapVersion_;
  }
  return {stickyVersion_, snapVersion_};
}

void LiveScroll::registerHandle(const std::shared_ptr<LiveScroll>& liveScroll) {
  if (!liveScroll) {
    return;
  }
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entries = registry();
  if (entries.find(liveScroll->getHandle()) != entries.end()) {
    return;
  }
  for (auto entry = entries.begin(); entry != entries.end();) {
    entry = entry->second.expired() ? entries.erase(entry) : std::next(entry);
  }
  entries.emplace(liveScroll->getHandle(), liveScroll);
}

std::shared_ptr<LiveScroll> LiveScroll::find(std::int64_t handle) {
  std::lock_guard<std::mutex> lock(registryMutex());
  auto& entries = registry();
  auto entry = entries.find(handle);
  return entry == entries.end() ? nullptr : entry->second.lock();
}

}
