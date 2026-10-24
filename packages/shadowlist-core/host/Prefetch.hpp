#pragma once

#include <shadowlist-core/host/MountedRange.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Rows a host tells its app to load early. Rows of the window that are not mounted get
 * prefetched once. A prefetched row that leaves the window before it was mounted is cancelled.
 * Rows are held by key, so an edit moves them along, and reported by index, low to high.
 * ListDriver uses the core's measured window. Fabric uses the mounted rows plus prefetchRows.
 */
class PrefetchTracker final {
public:
  /*
   * keyAt gives the key of a row below count, indexOfKey a key's row or UNDEFINED_INDEX.
   * Results go into prefetch and cancel, which are cleared first.
   */
  template <typename KeyAt, typename IndexOfKey>
  void update(
    const std::optional<MountedRange>& mounted,
    const std::optional<MountedRange>& window,
    std::size_t count,
    KeyAt keyAt,
    IndexOfKey indexOfKey,
    std::vector<std::size_t>& prefetch,
    std::vector<std::size_t>& cancel) {
    prefetch.clear();
    cancel.clear();
    auto isMounted = [&](std::size_t index) {
      return mounted && index >= mounted->low && index <= mounted->high;
    };
    auto inWindow = [&](std::size_t index) { return window && index >= window->low && index <= window->high; };
    // Rows prefetched earlier: shown now, gone, or out of the window.
    for (auto entry = prefetched_.begin(); entry != prefetched_.end();) {
      std::size_t index = indexOfKey(*entry);
      if (index == UNDEFINED_INDEX || isMounted(index)) {
        entry = prefetched_.erase(entry);
      } else if (!inWindow(index)) {
        cancel.push_back(index);
        entry = prefetched_.erase(entry);
      } else {
        ++entry;
      }
    }
    if (window) {
      for (std::size_t index = window->low; index <= window->high && index < count; ++index) {
        if (!isMounted(index) && prefetched_.insert(keyAt(index)).second) {
          prefetch.push_back(index);
        }
      }
    }
    std::sort(cancel.begin(), cancel.end());
  }

  void clear() {
    prefetched_.clear();
  }

private:
  std::unordered_set<std::string> prefetched_;
};

/*
 * The window to prefetch: the mounted rows plus rows on each side, or nothing without mounted
 * rows or prefetch rows.
 */
std::optional<MountedRange> prefetchWindow(const std::optional<MountedRange>& mounted, std::size_t rows, std::size_t count);

}
