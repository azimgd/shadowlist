/*
 * Vector tests of the list engine: every file in vectors/ replayed through the functions of
 * host/. The jest tests of shadowlist replay the same files through the TypeScript, which keeps
 * the two sides from drifting apart. A file in the folder without a VECTOR_TEST fails
 * every_vector_file_has_a_replay.
 */

#include "TestFramework.hpp"
#include "VectorHelpers.hpp"

#include <shadowlist-core/host/MountedRange.hpp>

#include <cstddef>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

MountedRange rangeOf(const Json& range) {
  return {range["low"].asIndex(), range["high"].asIndex()};
}

void checkRange(const MountedRange& range, const Json& output) {
  CHECK_EQ(indexToNumber(range.low), output["low"].number);
  CHECK_EQ(indexToNumber(range.high), output["high"].number);
}

}

VECTOR_TEST(mounted_range_vectors_replay, "mounted-range.json") {
  const Json& input = vector["input"];
  const Json& output = vector["output"];
  if (isVectorFor(vector, "initialMountedRange")) {
    checkRange(
      initialMountedRange(
        input["size"].asIndex(), input["initialCount"].asIndex(), input["inverted"].boolean, input["target"].asIndex(),
        input["overscanRows"].asIndex(), input["viewPosition"].number),
      output);
  } else if (isVectorFor(vector, "grownMountedRange")) {
    checkRange(
      grownMountedRange(
        rangeOf(input["edges"]), input["lowAtStart"].boolean, input["highAtEnd"].boolean, input["size"].asIndex(),
        input["overscanRowsLeading"].asIndex(), input["followTail"].boolean),
      output);
  } else if (isVectorFor(vector, "unionRangeIndices")) {
    std::vector<std::size_t> indices = unionRangeIndices(rangeOf(input["first"]), rangeOf(input["second"]));
    CHECK_EQ(indices.size(), output.size());
    for (std::size_t index = 0; index < indices.size(); ++index) {
      CHECK_EQ(indexToNumber(indices[index]), output[index].number);
    }
  } else {
    CHECK(isVectorFor(vector, "shouldReseedFromIndex"));
    CHECK_EQ(shouldReseedFromIndex(input["previousIndex"].asIndex(), input["nextIndex"].asIndex()), output.boolean);
  }
}

TEST(every_vector_file_has_a_replay) {
  checkEveryVectorFileReplayed(SHADOWLIST_VECTORS_DIR, "vectors");
}
