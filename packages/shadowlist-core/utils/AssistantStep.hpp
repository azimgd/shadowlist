#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A trimmed text segment. at remains the original, pre-trim UTF-16 cursor, as in the RN view.
 */
struct AssistantTextStep {
  std::string text;
  std::size_t at = 0;

  bool operator==(const AssistantTextStep& other) const { return text == other.text && at == other.at; }
};

/*
 * Consecutive calls with no nonblank text between them are one group, in stable position order.
 */
struct AssistantCallsStep {
  std::vector<std::size_t> callIndices;

  bool operator==(const AssistantCallsStep& other) const { return callIndices == other.callIndices; }
};

using AssistantStep = std::variant<AssistantTextStep, AssistantCallsStep>;

/*
 * The Assistant reply view's stepsOf rule. Content is UTF-8; call positions are integer UTF-16
 * offsets, as produced by the RN turn writer. Missing positions are passed as 0. Negative
 * positions sort before text and positive positions cap at its length. A split surrogate reads
 * as U+FFFD in each text segment. Nonfinite/fractional JS numbers are outside this typed API.
 */
std::vector<AssistantStep> stepsOf(std::string_view content, const std::vector<std::int64_t>& positions);

}
