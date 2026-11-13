#include "AssistantStep.hpp"

#include "Text.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

namespace azimgd::shadowlist {
namespace {

std::vector<char16_t> utf16(std::string_view content) {
  std::vector<char16_t> result;
  for (std::size_t at = 0; at < content.size();) {
    char32_t point = decodeUtf8(content, at);
    if (point <= 0xFFFF) result.push_back(static_cast<char16_t>(point));
    else {
      point -= 0x10000;
      result.push_back(static_cast<char16_t>(0xD800 + (point >> 10)));
      result.push_back(static_cast<char16_t>(0xDC00 + (point & 0x3FF)));
    }
  }
  return result;
}

std::string slice(const std::vector<char16_t>& content, std::size_t start, std::size_t end) {
  std::string result;
  for (std::size_t index = start; index < end; ++index) {
    char32_t point = content[index];
    if (point >= 0xD800 && point <= 0xDBFF && index + 1 < end &&
      content[index + 1] >= 0xDC00 && content[index + 1] <= 0xDFFF) {
      point = 0x10000 + ((point - 0xD800) << 10) + (content[++index] - 0xDC00);
    }
    appendUtf8(result, point);
  }
  return result;
}

}

std::vector<AssistantStep> stepsOf(std::string_view content, const std::vector<std::int64_t>& positions) {
  const auto text = utf16(content);
  std::vector<std::pair<std::int64_t, std::size_t>> ordered;
  ordered.reserve(positions.size());
  for (std::size_t index = 0; index < positions.size(); ++index) {
    ordered.emplace_back(std::min(positions[index], static_cast<std::int64_t>(text.size())), index);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto& first, const auto& second) {
    return first.first < second.first;
  });
  std::vector<AssistantStep> steps;
  std::size_t cursor = 0;
  auto pushText = [&](std::size_t end) {
    const auto part = slice(text, cursor, end);
    const auto trimmed = trim(part);
    if (!trimmed.empty()) steps.emplace_back(AssistantTextStep{std::string(trimmed), cursor});
    cursor = end;
  };
  for (const auto& entry : ordered) {
    if (entry.first > static_cast<std::int64_t>(cursor)) pushText(static_cast<std::size_t>(entry.first));
    if (!steps.empty()) {
      if (auto* calls = std::get_if<AssistantCallsStep>(&steps.back())) {
        calls->callIndices.push_back(entry.second);
        continue;
      }
    }
    steps.emplace_back(AssistantCallsStep{{entry.second}});
  }
  pushText(text.size());
  return steps;
}

}
