#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A plain JSON value for data that crosses from a host into the list, like ShadowListNative
 * items. Hosts convert their own values at the boundary, so nothing here needs folly or JSI.
 *
 * Numbers keep whole and fractional kinds apart, like folly::dynamic, but compare by value.
 * Objects keep insertion order and compare without regard to it. They are small, so lookups
 * walk the fields.
 */
class JsonValue {
public:
  enum class Type { Null, Bool, Int, Double, String, Array, Object };

  using Array = std::vector<JsonValue>;
  using Object = std::vector<std::pair<std::string, JsonValue>>;

  JsonValue() = default;
  JsonValue(std::nullptr_t) {}
  JsonValue(bool value) : value_(value) {}
  JsonValue(int value) : value_(static_cast<std::int64_t>(value)) {}
  JsonValue(std::int64_t value) : value_(value) {}
  JsonValue(double value) : value_(value) {}
  JsonValue(const char* value) : value_(std::string(value)) {}
  JsonValue(std::string value) : value_(std::move(value)) {}
  JsonValue(std::string_view value) : value_(std::string(value)) {}
  JsonValue(Array value) : value_(std::move(value)) {}
  JsonValue(Object value) : value_(std::move(value)) {}

  static JsonValue array() {
    return JsonValue(Array{});
  }

  static JsonValue object() {
    return JsonValue(Object{});
  }

  Type type() const {
    return static_cast<Type>(value_.index());
  }

  bool isNull() const { return type() == Type::Null; }
  bool isBool() const { return type() == Type::Bool; }
  bool isInt() const { return type() == Type::Int; }
  bool isDouble() const { return type() == Type::Double; }
  bool isNumber() const { return isInt() || isDouble(); }
  bool isString() const { return type() == Type::String; }
  bool isArray() const { return type() == Type::Array; }
  bool isObject() const { return type() == Type::Object; }

  bool getBool() const { return std::get<bool>(value_); }
  std::int64_t getInt() const { return std::get<std::int64_t>(value_); }
  double getDouble() const { return std::get<double>(value_); }
  const std::string& getString() const { return std::get<std::string>(value_); }
  const Array& getArray() const { return std::get<Array>(value_); }
  Array& getArray() { return std::get<Array>(value_); }
  const Object& items() const { return std::get<Object>(value_); }
  Object& items() { return std::get<Object>(value_); }

  /*
   * A number as a double, or 0 for anything else.
   */
  double asDouble() const;

  /*
   * A scalar as text the way folly::dynamic::asString writes it: whole numbers plainly,
   * doubles in their shortest form, true and false as 1 and 0. Empty for the rest.
   */
  std::string asString() const;

  /*
   * Entries of an array or fields of an object, 0 for anything else.
   */
  std::size_t size() const;
  bool empty() const { return size() == 0; }

  // An array entry. Callers check the index first.
  const JsonValue& operator[](std::size_t index) const { return getArray()[index]; }
  JsonValue& operator[](std::size_t index) { return getArray()[index]; }

  /*
   * An object's field, or null when this is not an object or has no such field.
   */
  const JsonValue* find(std::string_view key) const;

  /*
   * An object's field, added as null when missing. Turns a non object into an empty object first.
   */
  JsonValue& operator[](std::string_view key);

  // Drops an object's field. Returns whether it was there.
  bool erase(std::string_view key);

  void push_back(JsonValue value) { getArray().push_back(std::move(value)); }

  bool operator==(const JsonValue& other) const;
  bool operator!=(const JsonValue& other) const { return !(*this == other); }

  /*
   * Parses JSON text, or returns nothing when it is not valid JSON.
   */
  static std::optional<JsonValue> parse(std::string_view text);

private:
  std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object> value_{nullptr};
};

}
