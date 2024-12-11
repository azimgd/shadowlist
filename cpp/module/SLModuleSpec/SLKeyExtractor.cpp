#include "SLKeyExtractor.h"

namespace facebook::react {

std::optional<std::string> SLKeyExtractor::extractKey(const std::string& input) {
  if (input.length() < 4) {
    return std::nullopt;
  }

  if (input.substr(0, 2) != "{{" || input.substr(input.length() - 2) != "}}") {
    return std::nullopt;
  }

  std::string key = input.substr(2, input.length() - 4);

  if (key.find('{') != std::string::npos || key.find('}') != std::string::npos) {
    return std::nullopt;
  }

  if (key.empty()) {
    return std::nullopt;
  }

  return key;
}

std::vector<std::string> SLKeyExtractor::extractAllKeys(const std::string& input) {
  std::vector<std::string> keys;
  size_t pos = 0;
  
  while (pos < input.length()) {
    size_t start = input.find("{{", pos);
    if (start == std::string::npos) break;

    size_t end = input.find("}}", start);
    if (end == std::string::npos) break;

    std::string potential_key = input.substr(start, end - start + 2);
    auto key = extractKey(potential_key);

    if (key) {
      keys.push_back(*key);
    }

    pos = end + 2;
  }

  return keys;
}
}
