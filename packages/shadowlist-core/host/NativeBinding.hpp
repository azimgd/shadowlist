#pragma once

#include <shadowlist-core/host/JsonValue.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/*
 * ShadowListNative bindings. A binding fills a template prop from the row's item. For example:
 *   author.name reads that path, and images.0.uri reads into an array
 *   !isRead flips the value, for hidden and visible
 *   {name} {date} is a format string where each path in braces is filled in
 *   row+1 adds a number to the value when it is a number
 */
namespace azimgd::shadowlist {

struct NativeFormatPart {
  // Literal text when path is empty, otherwise the value at path.
  std::string text;
  std::vector<std::string> path;
  // Added to the value when it is a number, as in row+1.
  double offset = 0;
};

struct NativeExpression {
  bool negate = false;
  bool format = false;
  // One path part for a plain expression, text and path parts for a format.
  std::vector<NativeFormatPart> parts;
};

/*
 * Splits a trailing number off a path, so row+1 becomes row and 1.
 * It only counts when a sign follows the path and a whole number follows the sign.
 */
double splitNativeOffset(std::string_view& path);

std::vector<std::string> parseNativePath(std::string_view path);

NativeExpression parseNativeExpression(std::string_view source);

/*
 * Bound colors usually arrive as strings. Turn the common CSS forms into the integer Fabric
 * expects, or return nothing when the string is not a color.
 */
std::optional<std::uint32_t> parseNativeColor(std::string_view source);

/*
 * Whether a bound prop takes a color, so a string value must be parsed first.
 */
bool isNativeColorProp(std::string_view prop);

/*
 * A missing or null value, or a color that does not parse, keeps the template's own value.
 * Hidden and visible always apply, since null counts as false.
 */
bool nativeBindingKeepsTemplate(
  std::string_view prop,
  bool isNull,
  std::optional<std::string_view> string = std::nullopt);

/*
 * The value at path inside item, or null when any step is missing. Numeric steps index arrays.
 */
const JsonValue& lookupNativePath(const JsonValue& item, const std::vector<std::string>& path);

/*
 * JavaScript truthiness, except that every object counts as true and an empty array as false.
 */
bool nativeTruthy(const JsonValue& value);

/*
 * The text a value shows as. Null, objects and arrays show nothing.
 */
std::string nativeText(const JsonValue& value);

/*
 * The value at the part's path plus its offset when it is a number. A whole number result
 * stays an int.
 */
JsonValue nativePartValue(const NativeFormatPart& part, const JsonValue& item);

/*
 * What the expression gives for item: the joined text of a format, or the value, flipped
 * to a bool when negated.
 */
JsonValue evaluateNative(const NativeExpression& expression, const JsonValue& item);

}
