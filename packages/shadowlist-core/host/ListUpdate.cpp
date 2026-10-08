#include <shadowlist-core/host/ListUpdate.hpp>

#include <algorithm>

namespace azimgd::shadowlist {

namespace {

void sortUnique(std::vector<std::size_t>& indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
}

}

std::vector<std::size_t> insertionPositions(std::vector<std::size_t> indices, std::size_t previousCount) {
  sortUnique(indices);
  for (std::size_t rank = 0; rank < indices.size(); ++rank) {
    indices[rank] = std::min(indices[rank], previousCount + rank);
  }
  return indices;
}

std::vector<std::size_t> deletionPositions(std::vector<std::size_t> indices, std::size_t previousCount) {
  indices.erase(std::remove_if(indices.begin(), indices.end(),
    [previousCount](std::size_t index) { return index >= previousCount; }), indices.end());
  sortUnique(indices);
  return indices;
}

}
