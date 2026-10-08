#include <shadowlist-core/host/KeyDiff.hpp>

#include <algorithm>
#include <unordered_map>

namespace azimgd::shadowlist {

namespace {

/*
 * Positions in values of a longest strictly increasing run, ascending.
 */
std::vector<std::size_t> longestIncreasing(const std::vector<std::size_t>& values) {
  std::vector<std::size_t> tails;
  std::vector<std::size_t> parents(values.size(), UNDEFINED_INDEX);
  for (std::size_t at = 0; at < values.size(); ++at) {
    auto place = std::lower_bound(tails.begin(), tails.end(), values[at],
      [&values](std::size_t tail, std::size_t value) { return values[tail] < value; });
    if (place != tails.begin()) {
      parents[at] = *(place - 1);
    }
    if (place == tails.end()) {
      tails.push_back(at);
    } else {
      *place = at;
    }
  }
  std::vector<std::size_t> run;
  for (std::size_t at = tails.empty() ? UNDEFINED_INDEX : tails.back(); at != UNDEFINED_INDEX; at = parents[at]) {
    run.push_back(at);
  }
  std::reverse(run.begin(), run.end());
  return run;
}

}

KeyDiff diffKeys(const std::vector<std::string>& previous, const std::vector<std::string>& next) {
  KeyDiff diff;
  std::unordered_map<std::string, std::size_t> previousIndex;
  previousIndex.reserve(previous.size());
  for (std::size_t index = 0; index < previous.size(); ++index) {
    previousIndex.emplace(previous[index], index);
  }
  std::vector<bool> kept(previous.size(), false);
  // Kept keys in next order, with their previous index.
  std::vector<std::size_t> keptNext;
  std::vector<std::size_t> keptPrevious;
  for (std::size_t index = 0; index < next.size(); ++index) {
    auto found = previousIndex.find(next[index]);
    if (found == previousIndex.end() || kept[found->second]) {
      diff.inserted.push_back(index);
      continue;
    }
    kept[found->second] = true;
    keptNext.push_back(index);
    keptPrevious.push_back(found->second);
  }
  for (std::size_t index = 0; index < previous.size(); ++index) {
    if (!kept[index]) {
      diff.deleted.push_back(index);
    }
  }
  std::vector<std::size_t> run = longestIncreasing(keptPrevious);
  std::size_t inRun = 0;
  for (std::size_t at = 0; at < keptPrevious.size(); ++at) {
    if (inRun < run.size() && run[inRun] == at) {
      ++inRun;
      continue;
    }
    diff.moved.push_back({keptPrevious[at], keptNext[at]});
  }
  return diff;
}

std::optional<BatchPlan> planBatch(std::size_t previousCount, std::size_t nextCount, const BatchUpdate& batch) {
  std::vector<bool> leaves(previousCount, false);
  auto leave = [&](std::size_t index) {
    if (index >= previousCount || leaves[index]) {
      return false;
    }
    leaves[index] = true;
    return true;
  };
  for (std::size_t index : batch.deleted) {
    if (!leave(index)) {
      return std::nullopt;
    }
  }
  for (const KeyMove& move : batch.moved) {
    if (!leave(move.from)) {
      return std::nullopt;
    }
  }
  // Each filled place of the next data: the previous row a move brings, or UNDEFINED_INDEX for an insert.
  std::vector<std::pair<std::size_t, std::size_t>> filled;
  filled.reserve(batch.inserted.size() + batch.moved.size());
  for (std::size_t index : batch.inserted) {
    filled.emplace_back(index, UNDEFINED_INDEX);
  }
  for (const KeyMove& move : batch.moved) {
    filled.emplace_back(move.to, move.from);
  }
  std::sort(filled.begin(), filled.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (std::size_t at = 0; at < filled.size(); ++at) {
    if (filled[at].first >= nextCount || (at > 0 && filled[at].first == filled[at - 1].first)) {
      return std::nullopt;
    }
  }
  std::size_t staying = previousCount - static_cast<std::size_t>(std::count(leaves.begin(), leaves.end(), true));
  if (staying + filled.size() != nextCount) {
    return std::nullopt;
  }
  BatchPlan plan;
  plan.sources.reserve(nextCount);
  std::size_t source = 0;
  std::size_t fill = 0;
  for (std::size_t index = 0; index < nextCount; ++index) {
    if (fill < filled.size() && filled[fill].first == index) {
      plan.sources.push_back(filled[fill++].second);
      continue;
    }
    while (source < previousCount && leaves[source]) {
      ++source;
    }
    plan.sources.push_back(source++);
  }
  return plan;
}

}
