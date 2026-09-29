#include <shadowlist-core/host/JsonValue.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace azimgd::shadowlist {

namespace {

/*
 * The shortest text that reads back as the same double, laid out like folly's
 * to<std::string>(double): plain decimals for exponents from -6 to 20, else d.ddEx.
 */
std::string formatDouble(double value) {
  if (std::isnan(value)) {
    return "NaN";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-Infinity" : "Infinity";
  }
  char buffer[40];
  for (int precision = 1; precision <= 17; ++precision) {
    std::snprintf(buffer, sizeof(buffer), "%.*e", precision - 1, value);
    if (std::strtod(buffer, nullptr) == value) {
      break;
    }
  }
  // buffer holds [-]d[.ddd]e[+-]xx.
  std::string scientific(buffer);
  bool negative = scientific.front() == '-';
  std::size_t exponentAt = scientific.find('e');
  std::string mantissa = scientific.substr(negative ? 1 : 0, exponentAt - (negative ? 1 : 0));
  int exponent = std::atoi(scientific.c_str() + exponentAt + 1);
  std::string digits;
  for (char character : mantissa) {
    if (character != '.') {
      digits.push_back(character);
    }
  }
  while (digits.size() > 1 && digits.back() == '0') {
    digits.pop_back();
  }

  std::string text = negative ? "-" : "";
  if (exponent >= -6 && exponent <= 20) {
    int point = exponent + 1;
    if (point <= 0) {
      text += "0.";
      text.append(static_cast<std::size_t>(-point), '0');
      text += digits;
    } else if (static_cast<std::size_t>(point) >= digits.size()) {
      text += digits;
      text.append(static_cast<std::size_t>(point) - digits.size(), '0');
    } else {
      text += digits.substr(0, static_cast<std::size_t>(point));
      text += '.';
      text += digits.substr(static_cast<std::size_t>(point));
    }
    return text;
  }
  text += digits.substr(0, 1);
  if (digits.size() > 1) {
    text += '.';
    text += digits.substr(1);
  }
  text += 'E';
  text += std::to_string(exponent);
  return text;
}

class Parser {
public:
  explicit Parser(std::string_view text) : text_(text) {}

  std::optional<JsonValue> document() {
    auto value = parseValue(0);
    skipSpace();
    if (!value || position_ != text_.size()) {
      return std::nullopt;
    }
    return value;
  }

private:
  // Deeper nesting than any real item is refused instead of overflowing the stack.
  static constexpr int MAX_DEPTH = 256;

  void skipSpace() {
    while (position_ < text_.size() &&
           (text_[position_] == ' ' || text_[position_] == '\n' || text_[position_] == '\r' || text_[position_] == '\t')) {
      ++position_;
    }
  }

  bool consume(std::string_view word) {
    if (text_.substr(position_, word.size()) != word) {
      return false;
    }
    position_ += word.size();
    return true;
  }

  std::optional<JsonValue> parseValue(int depth) {
    if (depth > MAX_DEPTH) {
      return std::nullopt;
    }
    skipSpace();
    if (position_ >= text_.size()) {
      return std::nullopt;
    }
    char character = text_[position_];
    if (character == '{') {
      return parseObject(depth);
    }
    if (character == '[') {
      return parseArray(depth);
    }
    if (character == '"') {
      auto text = parseString();
      return text ? std::optional<JsonValue>(JsonValue(std::move(*text))) : std::nullopt;
    }
    if (consume("true")) {
      return JsonValue(true);
    }
    if (consume("false")) {
      return JsonValue(false);
    }
    if (consume("null")) {
      return JsonValue(nullptr);
    }
    return parseNumber();
  }

  std::optional<JsonValue> parseObject(int depth) {
    ++position_;
    JsonValue::Object fields;
    skipSpace();
    if (position_ < text_.size() && text_[position_] == '}') {
      ++position_;
      return JsonValue(std::move(fields));
    }
    while (true) {
      skipSpace();
      if (position_ >= text_.size() || text_[position_] != '"') {
        return std::nullopt;
      }
      auto key = parseString();
      skipSpace();
      if (!key || position_ >= text_.size() || text_[position_] != ':') {
        return std::nullopt;
      }
      ++position_;
      auto value = parseValue(depth + 1);
      if (!value) {
        return std::nullopt;
      }
      // A repeated key keeps the last value, like JSON.parse.
      bool replaced = false;
      for (auto& field : fields) {
        if (field.first == *key) {
          field.second = std::move(*value);
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        fields.emplace_back(std::move(*key), std::move(*value));
      }
      skipSpace();
      if (position_ < text_.size() && text_[position_] == ',') {
        ++position_;
        continue;
      }
      if (position_ < text_.size() && text_[position_] == '}') {
        ++position_;
        return JsonValue(std::move(fields));
      }
      return std::nullopt;
    }
  }

  std::optional<JsonValue> parseArray(int depth) {
    ++position_;
    JsonValue::Array entries;
    skipSpace();
    if (position_ < text_.size() && text_[position_] == ']') {
      ++position_;
      return JsonValue(std::move(entries));
    }
    while (true) {
      auto value = parseValue(depth + 1);
      if (!value) {
        return std::nullopt;
      }
      entries.push_back(std::move(*value));
      skipSpace();
      if (position_ < text_.size() && text_[position_] == ',') {
        ++position_;
        continue;
      }
      if (position_ < text_.size() && text_[position_] == ']') {
        ++position_;
        return JsonValue(std::move(entries));
      }
      return std::nullopt;
    }
  }

  static void appendUtf8(std::string& out, std::uint32_t codePoint) {
    if (codePoint < 0x80) {
      out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
  }

  std::optional<std::uint32_t> parseHex4() {
    if (position_ + 4 > text_.size()) {
      return std::nullopt;
    }
    std::uint32_t value = 0;
    for (int index = 0; index < 4; ++index) {
      char character = text_[position_++];
      value <<= 4;
      if (character >= '0' && character <= '9') {
        value |= static_cast<std::uint32_t>(character - '0');
      } else if (character >= 'a' && character <= 'f') {
        value |= static_cast<std::uint32_t>(character - 'a' + 10);
      } else if (character >= 'A' && character <= 'F') {
        value |= static_cast<std::uint32_t>(character - 'A' + 10);
      } else {
        return std::nullopt;
      }
    }
    return value;
  }

  std::optional<std::string> parseString() {
    ++position_;
    std::string out;
    while (position_ < text_.size()) {
      char character = text_[position_++];
      if (character == '"') {
        return out;
      }
      if (character != '\\') {
        out.push_back(character);
        continue;
      }
      if (position_ >= text_.size()) {
        return std::nullopt;
      }
      char escape = text_[position_++];
      switch (escape) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          auto high = parseHex4();
          if (!high) {
            return std::nullopt;
          }
          std::uint32_t codePoint = *high;
          // A surrogate pair joins into one code point.
          if (codePoint >= 0xD800 && codePoint <= 0xDBFF && text_.substr(position_, 2) == "\\u") {
            position_ += 2;
            auto low = parseHex4();
            if (!low) {
              return std::nullopt;
            }
            codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (*low - 0xDC00);
          }
          appendUtf8(out, codePoint);
          break;
        }
        default:
          return std::nullopt;
      }
    }
    return std::nullopt;
  }

  std::optional<JsonValue> parseNumber() {
    std::size_t start = position_;
    bool fractional = false;
    if (position_ < text_.size() && text_[position_] == '-') {
      ++position_;
    }
    while (position_ < text_.size()) {
      char character = text_[position_];
      if (character >= '0' && character <= '9') {
        ++position_;
      } else if (character == '.' || character == 'e' || character == 'E' || character == '+' || character == '-') {
        fractional = true;
        ++position_;
      } else {
        break;
      }
    }
    std::string number(text_.substr(start, position_ - start));
    if (number.empty() || number == "-") {
      return std::nullopt;
    }
    char* end = nullptr;
    if (!fractional) {
      long long whole = std::strtoll(number.c_str(), &end, 10);
      if (end != nullptr && *end == '\0') {
        return JsonValue(static_cast<std::int64_t>(whole));
      }
    }
    double value = std::strtod(number.c_str(), &end);
    if (end == nullptr || *end != '\0') {
      return std::nullopt;
    }
    return JsonValue(value);
  }

  std::string_view text_;
  std::size_t position_ = 0;
};

}

double JsonValue::asDouble() const {
  switch (type()) {
    case Type::Int:
      return static_cast<double>(getInt());
    case Type::Double:
      return getDouble();
    default:
      return 0.0;
  }
}

std::string JsonValue::asString() const {
  switch (type()) {
    case Type::Bool:
      return getBool() ? "1" : "0";
    case Type::Int:
      return std::to_string(getInt());
    case Type::Double:
      return formatDouble(getDouble());
    case Type::String:
      return getString();
    default:
      return {};
  }
}

std::size_t JsonValue::size() const {
  switch (type()) {
    case Type::Array:
      return getArray().size();
    case Type::Object:
      return items().size();
    default:
      return 0;
  }
}

const JsonValue* JsonValue::find(std::string_view key) const {
  if (!isObject()) {
    return nullptr;
  }
  for (const auto& field : items()) {
    if (field.first == key) {
      return &field.second;
    }
  }
  return nullptr;
}

JsonValue& JsonValue::operator[](std::string_view key) {
  if (!isObject()) {
    value_ = Object{};
  }
  auto& fields = items();
  for (auto& field : fields) {
    if (field.first == key) {
      return field.second;
    }
  }
  fields.emplace_back(std::string(key), JsonValue());
  return fields.back().second;
}

bool JsonValue::erase(std::string_view key) {
  if (!isObject()) {
    return false;
  }
  auto& fields = items();
  for (auto iterator = fields.begin(); iterator != fields.end(); ++iterator) {
    if (iterator->first == key) {
      fields.erase(iterator);
      return true;
    }
  }
  return false;
}

bool JsonValue::operator==(const JsonValue& other) const {
  if (isNumber() && other.isNumber()) {
    if (isInt() && other.isInt()) {
      return getInt() == other.getInt();
    }
    return asDouble() == other.asDouble();
  }
  if (type() != other.type()) {
    return false;
  }
  switch (type()) {
    case Type::Null:
      return true;
    case Type::Bool:
      return getBool() == other.getBool();
    case Type::String:
      return getString() == other.getString();
    case Type::Array:
      return getArray() == other.getArray();
    case Type::Object: {
      if (items().size() != other.items().size()) {
        return false;
      }
      for (const auto& field : items()) {
        const JsonValue* match = other.find(field.first);
        if (match == nullptr || *match != field.second) {
          return false;
        }
      }
      return true;
    }
    default:
      return false;
  }
}

std::optional<JsonValue> JsonValue::parse(std::string_view text) {
  return Parser(text).document();
}

}
