#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/*
 * Parses ShadowListNative bindings. No folly or React here, so the core tests can use it.
 * A binding fills a template prop from the row's item. For example:
 *   author.name reads that path, and images.0.uri reads into an array
 *   !isRead flips the value, for hidden and visible
 *   {name} {date} is a format string where each path in braces is filled in
 *   row+1 adds a number to the value when it is a number
 */
namespace facebook::react {

struct ShadowListNativeFormatPart {
  // Literal text when path is empty, otherwise the value at path.
  std::string text;
  std::vector<std::string> path;
  // Added to the value when it is a number, as in row+1.
  double offset = 0;
};

/*
 * Splits a trailing number off a path, so row+1 becomes row and 1.
 * It only counts when a sign follows the path and a whole number follows the sign.
 */
inline double splitShadowListNativeOffset(std::string_view& path) {
  std::size_t sign = path.find_last_of("+-");
  if (sign == std::string_view::npos || sign == 0 || sign + 1 >= path.size()) {
    return 0;
  }
  double value = 0;
  for (std::size_t index = sign + 1; index < path.size(); ++index) {
    char character = path[index];
    if (character < '0' || character > '9') {
      return 0;
    }
    value = value * 10 + (character - '0');
  }
  bool negative = path[sign] == '-';
  path = path.substr(0, sign);
  return negative ? -value : value;
}

struct ShadowListNativeExpression {
  bool negate = false;
  bool format = false;
  // One path part for a plain expression, text and path parts for a format.
  std::vector<ShadowListNativeFormatPart> parts;
};

inline std::vector<std::string> parseShadowListNativePath(std::string_view path) {
  std::vector<std::string> segments;
  std::size_t start = 0;
  while (start <= path.size()) {
    std::size_t dot = path.find('.', start);
    if (dot == std::string_view::npos) {
      dot = path.size();
    }
    if (dot > start) {
      segments.emplace_back(path.substr(start, dot - start));
    }
    start = dot + 1;
  }
  return segments;
}

inline ShadowListNativeExpression parseShadowListNativeExpression(std::string_view source) {
  ShadowListNativeExpression expression;
  if (source.find('{') != std::string_view::npos) {
    expression.format = true;
    std::string literal;
    std::size_t index = 0;
    while (index < source.size()) {
      char character = source[index];
      if (character == '{') {
        std::size_t close = source.find('}', index + 1);
        if (close == std::string_view::npos) {
          literal.append(source.substr(index));
          break;
        }
        if (!literal.empty()) {
          expression.parts.push_back({std::move(literal), {}});
          literal.clear();
        }
        auto inner = source.substr(index + 1, close - index - 1);
        double offset = splitShadowListNativeOffset(inner);
        auto path = parseShadowListNativePath(inner);
        if (!path.empty()) {
          expression.parts.push_back({{}, std::move(path), offset});
        }
        index = close + 1;
        continue;
      }
      literal.push_back(character);
      ++index;
    }
    if (!literal.empty()) {
      expression.parts.push_back({std::move(literal), {}});
    }
    return expression;
  }

  if (!source.empty() && source.front() == '!') {
    expression.negate = true;
    source.remove_prefix(1);
  }
  double offset = splitShadowListNativeOffset(source);
  expression.parts.push_back({{}, parseShadowListNativePath(source), offset});
  return expression;
}

/*
 * Bound colors usually arrive as strings. Turn the common CSS forms into the integer Fabric
 * expects, or return nothing when the string is not a color.
 */
inline std::optional<std::uint32_t> parseShadowListNativeColor(std::string_view source) {
  auto hexValue = [](char character) -> int {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
  };
  auto pack = [](std::uint32_t r, std::uint32_t g, std::uint32_t b, std::uint32_t a) -> std::uint32_t {
    return (a << 24) | (r << 16) | (g << 8) | b;
  };

  while (!source.empty() && source.front() == ' ') source.remove_prefix(1);
  while (!source.empty() && source.back() == ' ') source.remove_suffix(1);

  if (source == "transparent") {
    return 0u;
  }
  if (source == "black") return pack(0, 0, 0, 255);
  if (source == "white") return pack(255, 255, 255, 255);
  if (source == "red") return pack(255, 0, 0, 255);
  if (source == "green") return pack(0, 128, 0, 255);
  if (source == "blue") return pack(0, 0, 255, 255);

  if (!source.empty() && source.front() == '#') {
    auto digits = source.substr(1);
    std::vector<int> values;
    for (char character : digits) {
      int value = hexValue(character);
      if (value < 0) return std::nullopt;
      values.push_back(value);
    }
    if (values.size() == 3 || values.size() == 4) {
      std::uint32_t r = values[0] * 17, g = values[1] * 17, b = values[2] * 17;
      std::uint32_t a = values.size() == 4 ? values[3] * 17 : 255;
      return pack(r, g, b, a);
    }
    if (values.size() == 6 || values.size() == 8) {
      std::uint32_t r = values[0] * 16 + values[1], g = values[2] * 16 + values[3], b = values[4] * 16 + values[5];
      std::uint32_t a = values.size() == 8 ? values[6] * 16 + values[7] : 255;
      return pack(r, g, b, a);
    }
    return std::nullopt;
  }

  bool rgba = source.rfind("rgba(", 0) == 0;
  bool rgb = source.rfind("rgb(", 0) == 0;
  if ((rgba || rgb) && source.back() == ')') {
    auto body = source.substr(rgba ? 5 : 4);
    body.remove_suffix(1);
    std::vector<double> components;
    std::string current;
    auto flush = [&]() -> bool {
      std::size_t first = current.find_first_not_of(' ');
      if (first == std::string::npos) return false;
      try {
        components.push_back(std::stod(current.substr(first)));
      } catch (...) {
        return false;
      }
      current.clear();
      return true;
    };
    for (char character : body) {
      if (character == ',') {
        if (!flush()) return std::nullopt;
      } else {
        current.push_back(character);
      }
    }
    if (!flush()) return std::nullopt;
    if (components.size() != 3 && components.size() != 4) return std::nullopt;
    auto channel = [](double value) -> std::uint32_t {
      return value <= 0 ? 0u : value >= 255 ? 255u : static_cast<std::uint32_t>(value + 0.5);
    };
    double alpha = components.size() == 4 ? components[3] : 1.0;
    return pack(channel(components[0]), channel(components[1]), channel(components[2]), channel(alpha * 255.0));
  }
  return std::nullopt;
}

/*
 * Whether a bound prop takes a color, so a string value must be parsed first.
 */
inline bool isShadowListNativeColorProp(std::string_view prop) {
  constexpr std::string_view suffix = "Color";
  return prop == "color" ||
    (prop.size() > suffix.size() && prop.compare(prop.size() - suffix.size(), suffix.size(), suffix) == 0);
}

/*
 * A missing or null value, or a color that does not parse, keeps the template's own value.
 * Hidden and visible always apply, since null counts as false.
 */
inline bool shadowListNativeBindingKeepsTemplate(
  std::string_view prop,
  bool isNull,
  std::optional<std::string_view> string = std::nullopt) {
  if (prop == "hidden" || prop == "visible") {
    return false;
  }
  if (isNull) {
    return true;
  }
  return string && isShadowListNativeColorProp(prop) && !parseShadowListNativeColor(*string).has_value();
}

}
