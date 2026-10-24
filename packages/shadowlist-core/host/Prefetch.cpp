#include <shadowlist-core/host/Prefetch.hpp>

#include <algorithm>

namespace azimgd::shadowlist {

std::optional<MountedRange> prefetchRange(const std::optional<MountedRange>& mounted, std::size_t rows, std::size_t count) {
  if (!mounted || mounted->isEmpty() || count == 0 || rows == 0) {
    return std::nullopt;
  }
  std::size_t low = mounted->low > rows ? mounted->low - rows : 0;
  return MountedRange{low, std::min(count - 1, mounted->high + rows)};
}

}
