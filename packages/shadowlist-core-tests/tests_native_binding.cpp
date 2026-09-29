/*
 * Tests for ShadowListNative binding parsing: the expressions a template binds props to,
 * and the color strings the bound data can carry.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/NativeBinding.hpp>

#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(nativeBindingParsesDottedPaths) {
  auto path = parseNativePath("author.images.0.uri");
  CHECK_EQ(path.size(), std::size_t{4});
  CHECK_EQ(path[0], std::string("author"));
  CHECK_EQ(path[2], std::string("0"));
  CHECK_EQ(path[3], std::string("uri"));
  // Empty path segments are skipped.
  CHECK_EQ(parseNativePath(".a..b.").size(), std::size_t{2});
  CHECK(parseNativePath("").empty());
}

TEST(nativeBindingParsesPlainAndNegatedExpressions) {
  auto plain = parseNativeExpression("author.name");
  CHECK(!plain.negate);
  CHECK(!plain.format);
  CHECK_EQ(plain.parts.size(), std::size_t{1});
  CHECK_EQ(plain.parts[0].path.size(), std::size_t{2});

  auto negated = parseNativeExpression("!images.1");
  CHECK(negated.negate);
  CHECK_EQ(negated.parts[0].path[0], std::string("images"));
  CHECK_EQ(negated.parts[0].path[1], std::string("1"));
}

TEST(nativeBindingParsesNumericOffsets) {
  auto plain = parseNativeExpression("row+1");
  CHECK_EQ(plain.parts[0].path.size(), std::size_t{1});
  CHECK_EQ(plain.parts[0].path[0], std::string("row"));
  CHECK_EQ(plain.parts[0].offset, 1.0);

  auto negative = parseNativeExpression("a.b-12");
  CHECK_EQ(negative.parts[0].path[1], std::string("b"));
  CHECK_EQ(negative.parts[0].offset, -12.0);

  auto format = parseNativeExpression("Row {row+1} of {count}");
  CHECK_EQ(format.parts[1].path[0], std::string("row"));
  CHECK_EQ(format.parts[1].offset, 1.0);
  CHECK_EQ(format.parts[3].offset, 0.0);

  // A sign at the start of the path or before a non number is not an offset.
  CHECK_EQ(parseNativeExpression("+1").parts[0].offset, 0.0);
  CHECK_EQ(parseNativeExpression("a+b").parts[0].path[0], std::string("a+b"));
  CHECK_EQ(parseNativeExpression("a+").parts[0].path[0], std::string("a+"));
}

TEST(nativeBindingParsesFormatStrings) {
  auto expression = parseNativeExpression("{name} · {stats.likes} likes");
  CHECK(expression.format);
  CHECK(!expression.negate);
  CHECK_EQ(expression.parts.size(), std::size_t{4});
  CHECK_EQ(expression.parts[0].path[0], std::string("name"));
  CHECK_EQ(expression.parts[1].text, std::string(" · "));
  CHECK_EQ(expression.parts[2].path.size(), std::size_t{2});
  CHECK_EQ(expression.parts[3].text, std::string(" likes"));

  // An unclosed brace stays as plain text.
  auto unclosed = parseNativeExpression("a {b");
  CHECK_EQ(unclosed.parts.size(), std::size_t{1});
  CHECK_EQ(unclosed.parts[0].text, std::string("a {b"));
}

TEST(nativeBindingParsesColors) {
  CHECK_EQ(*parseNativeColor("#fff"), 0xFFFFFFFFu);
  CHECK_EQ(*parseNativeColor("#0A84FF"), 0xFF0A84FFu);
  CHECK_EQ(*parseNativeColor("#0A84FF80"), 0x800A84FFu);
  CHECK_EQ(*parseNativeColor("#f008"), 0x88FF0000u);
  CHECK_EQ(*parseNativeColor("rgb(255, 0, 0)"), 0xFFFF0000u);
  CHECK_EQ(*parseNativeColor("rgba(235,235,245,0.6)"), 0x99EBEBF5u);
  CHECK_EQ(*parseNativeColor("transparent"), 0u);
  CHECK(!parseNativeColor("#12").has_value());
  CHECK(!parseNativeColor("#zzzzzz").has_value());
  CHECK(!parseNativeColor("rgb(1,2)").has_value());
  CHECK(!parseNativeColor("chartreuse").has_value());
}

TEST(nativeBindingRecognisesColorProps) {
  CHECK(isNativeColorProp("color"));
  CHECK(isNativeColorProp("backgroundColor"));
  CHECK(isNativeColorProp("borderTopColor"));
  CHECK(!isNativeColorProp("Color"));
  CHECK(!isNativeColorProp("opacity"));
}

TEST(nativeBindingMissingValuesKeepTheTemplateValue) {
  // A missing or null value keeps the template's own prop, like its color.
  CHECK(nativeBindingKeepsTemplate("color", true));
  CHECK(nativeBindingKeepsTemplate("borderColor", true));
  CHECK(nativeBindingKeepsTemplate("opacity", true));
  CHECK(nativeBindingKeepsTemplate("uri", true));
  // So does a color string that does not parse, even an empty one.
  CHECK(nativeBindingKeepsTemplate("color", false, std::string_view("")));
  CHECK(nativeBindingKeepsTemplate("backgroundColor", false, std::string_view("chartreuse")));
  CHECK(!nativeBindingKeepsTemplate("color", false, std::string_view("#fff")));
  // Present values apply, including strings for props that are not colors.
  CHECK(!nativeBindingKeepsTemplate("opacity", false));
  CHECK(!nativeBindingKeepsTemplate("uri", false, std::string_view("")));
  CHECK(!nativeBindingKeepsTemplate("nativeID", false, std::string_view("x")));
  // Hidden and visible always apply, and null counts as false.
  CHECK(!nativeBindingKeepsTemplate("hidden", true));
  CHECK(!nativeBindingKeepsTemplate("visible", true));
}

namespace {

JsonValue parsed(const char* text) {
  auto value = JsonValue::parse(text);
  if (!value) {
    ::slt::fail(std::string("bad JSON in test: ") + text);
  }
  return *value;
}

}

TEST(nativeBindingLooksUpPathsIntoObjectsAndArrays) {
  auto item = parsed(R"({"author":{"name":"Ada"},"images":[{"uri":"a.png"},{"uri":"b.png"}]})");
  CHECK_EQ(lookupNativePath(item, parseNativePath("author.name")).getString(), std::string("Ada"));
  CHECK_EQ(lookupNativePath(item, parseNativePath("images.1.uri")).getString(), std::string("b.png"));
  // Out of range, negative, non numeric and missing steps give null.
  CHECK(lookupNativePath(item, parseNativePath("images.2.uri")).isNull());
  CHECK(lookupNativePath(item, parseNativePath("images.-1")).isNull());
  CHECK(lookupNativePath(item, parseNativePath("images.x")).isNull());
  CHECK(lookupNativePath(item, parseNativePath("author.name.first")).isNull());
  CHECK(lookupNativePath(item, parseNativePath("missing")).isNull());
  // An empty path is the item itself.
  CHECK(lookupNativePath(item, {}) == item);
}

TEST(nativeBindingEvaluatesPlainNegatedAndOffsetExpressions) {
  auto item = parsed(R"({"row":11,"half":1.5,"isRead":false,"tags":[],"name":"x"})");
  CHECK_EQ(evaluateNative(parseNativeExpression("name"), item).getString(), std::string("x"));
  CHECK(evaluateNative(parseNativeExpression("!isRead"), item).getBool());
  CHECK(evaluateNative(parseNativeExpression("!tags"), item).getBool());
  CHECK(!evaluateNative(parseNativeExpression("!name"), item).getBool());
  CHECK(evaluateNative(parseNativeExpression("!missing"), item).getBool());

  // A whole result of an offset stays an int, a fractional one a double.
  auto next = evaluateNative(parseNativeExpression("row+1"), item);
  CHECK(next.isInt());
  CHECK_EQ(next.getInt(), std::int64_t{12});
  auto shifted = evaluateNative(parseNativeExpression("half+1"), item);
  CHECK(shifted.isDouble());
  CHECK_EQ(shifted.getDouble(), 2.5);
  // An offset on a string leaves it alone.
  CHECK_EQ(evaluateNative(parseNativeExpression("name+1"), item).getString(), std::string("x"));
}

TEST(nativeBindingFormatsText) {
  auto item = parsed(R"({"row":11,"ratio":0.25,"big":1e21,"on":true,"name":"Ada","obj":{},"list":[1]})");
  auto text = [&](const char* source) { return evaluateNative(parseNativeExpression(source), item).getString(); };
  CHECK_EQ(text("Row {row+1} of {name}"), std::string("Row 12 of Ada"));
  CHECK_EQ(text("{ratio}"), std::string("0.25"));
  CHECK_EQ(text("{big}"), std::string("1E21"));
  // Missing values, objects and arrays show nothing. Bools show like folly, as 1 and 0.
  CHECK_EQ(text("[{missing}{obj}{list}]"), std::string("[]"));
  CHECK_EQ(text("{on}"), std::string("1"));
}

TEST(nativeBindingTruthyAndTextOfEachType) {
  CHECK(!nativeTruthy(JsonValue()));
  CHECK(!nativeTruthy(JsonValue(false)));
  CHECK(nativeTruthy(JsonValue(true)));
  CHECK(!nativeTruthy(JsonValue(0)));
  CHECK(nativeTruthy(JsonValue(-3)));
  CHECK(!nativeTruthy(JsonValue(0.0)));
  CHECK(nativeTruthy(JsonValue(0.5)));
  CHECK(!nativeTruthy(JsonValue("")));
  CHECK(nativeTruthy(JsonValue("a")));
  CHECK(!nativeTruthy(JsonValue::array()));
  CHECK(nativeTruthy(parsed("[0]")));
  CHECK(nativeTruthy(JsonValue::object()));

  CHECK_EQ(nativeText(JsonValue()), std::string());
  CHECK_EQ(nativeText(JsonValue(7)), std::string("7"));
  CHECK_EQ(nativeText(JsonValue(3.0)), std::string("3"));
  CHECK_EQ(nativeText(JsonValue(-0.1)), std::string("-0.1"));
  CHECK_EQ(nativeText(JsonValue(0.000001)), std::string("0.000001"));
  CHECK_EQ(nativeText(JsonValue(1e-7)), std::string("1E-7"));
  CHECK_EQ(nativeText(JsonValue(123456789.125)), std::string("123456789.125"));
  CHECK_EQ(nativeText(JsonValue(false)), std::string("0"));
  CHECK_EQ(nativeText(JsonValue("hi")), std::string("hi"));
  CHECK_EQ(nativeText(parsed(R"({"a":1})")), std::string());
}
