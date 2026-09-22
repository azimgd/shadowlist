/*
 * Tests for ShadowListNative binding parsing: the expressions a template binds props to,
 * and the color strings the bound data can carry.
 */

#include "TestFramework.hpp"

#include <ShadowListNativeBinding.h>

#include <string>
#include <vector>

using namespace slt;
using namespace facebook::react;

TEST(nativeBindingParsesDottedPaths) {
  auto path = parseShadowListNativePath("author.images.0.uri");
  CHECK_EQ(path.size(), std::size_t{4});
  CHECK_EQ(path[0], std::string("author"));
  CHECK_EQ(path[2], std::string("0"));
  CHECK_EQ(path[3], std::string("uri"));
  // Empty path segments are skipped.
  CHECK_EQ(parseShadowListNativePath(".a..b.").size(), std::size_t{2});
  CHECK(parseShadowListNativePath("").empty());
}

TEST(nativeBindingParsesPlainAndNegatedExpressions) {
  auto plain = parseShadowListNativeExpression("author.name");
  CHECK(!plain.negate);
  CHECK(!plain.format);
  CHECK_EQ(plain.parts.size(), std::size_t{1});
  CHECK_EQ(plain.parts[0].path.size(), std::size_t{2});

  auto negated = parseShadowListNativeExpression("!images.1");
  CHECK(negated.negate);
  CHECK_EQ(negated.parts[0].path[0], std::string("images"));
  CHECK_EQ(negated.parts[0].path[1], std::string("1"));
}

TEST(nativeBindingParsesNumericOffsets) {
  auto plain = parseShadowListNativeExpression("row+1");
  CHECK_EQ(plain.parts[0].path.size(), std::size_t{1});
  CHECK_EQ(plain.parts[0].path[0], std::string("row"));
  CHECK_EQ(plain.parts[0].offset, 1.0);

  auto negative = parseShadowListNativeExpression("a.b-12");
  CHECK_EQ(negative.parts[0].path[1], std::string("b"));
  CHECK_EQ(negative.parts[0].offset, -12.0);

  auto format = parseShadowListNativeExpression("Row {row+1} of {count}");
  CHECK_EQ(format.parts[1].path[0], std::string("row"));
  CHECK_EQ(format.parts[1].offset, 1.0);
  CHECK_EQ(format.parts[3].offset, 0.0);

  // A sign at the start of the path or before a non number is not an offset.
  CHECK_EQ(parseShadowListNativeExpression("+1").parts[0].offset, 0.0);
  CHECK_EQ(parseShadowListNativeExpression("a+b").parts[0].path[0], std::string("a+b"));
  CHECK_EQ(parseShadowListNativeExpression("a+").parts[0].path[0], std::string("a+"));
}

TEST(nativeBindingParsesFormatStrings) {
  auto expression = parseShadowListNativeExpression("{name} · {stats.likes} likes");
  CHECK(expression.format);
  CHECK(!expression.negate);
  CHECK_EQ(expression.parts.size(), std::size_t{4});
  CHECK_EQ(expression.parts[0].path[0], std::string("name"));
  CHECK_EQ(expression.parts[1].text, std::string(" · "));
  CHECK_EQ(expression.parts[2].path.size(), std::size_t{2});
  CHECK_EQ(expression.parts[3].text, std::string(" likes"));

  // An unclosed brace stays as plain text.
  auto unclosed = parseShadowListNativeExpression("a {b");
  CHECK_EQ(unclosed.parts.size(), std::size_t{1});
  CHECK_EQ(unclosed.parts[0].text, std::string("a {b"));
}

TEST(nativeBindingParsesColors) {
  CHECK_EQ(*parseShadowListNativeColor("#fff"), 0xFFFFFFFFu);
  CHECK_EQ(*parseShadowListNativeColor("#0A84FF"), 0xFF0A84FFu);
  CHECK_EQ(*parseShadowListNativeColor("#0A84FF80"), 0x800A84FFu);
  CHECK_EQ(*parseShadowListNativeColor("#f008"), 0x88FF0000u);
  CHECK_EQ(*parseShadowListNativeColor("rgb(255, 0, 0)"), 0xFFFF0000u);
  CHECK_EQ(*parseShadowListNativeColor("rgba(235,235,245,0.6)"), 0x99EBEBF5u);
  CHECK_EQ(*parseShadowListNativeColor("transparent"), 0u);
  CHECK(!parseShadowListNativeColor("#12").has_value());
  CHECK(!parseShadowListNativeColor("#zzzzzz").has_value());
  CHECK(!parseShadowListNativeColor("rgb(1,2)").has_value());
  CHECK(!parseShadowListNativeColor("chartreuse").has_value());
}

TEST(nativeBindingRecognisesColorProps) {
  CHECK(isShadowListNativeColorProp("color"));
  CHECK(isShadowListNativeColorProp("backgroundColor"));
  CHECK(isShadowListNativeColorProp("borderTopColor"));
  CHECK(!isShadowListNativeColorProp("Color"));
  CHECK(!isShadowListNativeColorProp("opacity"));
}

TEST(nativeBindingMissingValuesKeepTheTemplateValue) {
  // A missing or null value keeps the template's own prop, like its color.
  CHECK(shadowListNativeBindingKeepsTemplate("color", true));
  CHECK(shadowListNativeBindingKeepsTemplate("borderColor", true));
  CHECK(shadowListNativeBindingKeepsTemplate("opacity", true));
  CHECK(shadowListNativeBindingKeepsTemplate("uri", true));
  // So does a color string that does not parse, even an empty one.
  CHECK(shadowListNativeBindingKeepsTemplate("color", false, std::string_view("")));
  CHECK(shadowListNativeBindingKeepsTemplate("backgroundColor", false, std::string_view("chartreuse")));
  CHECK(!shadowListNativeBindingKeepsTemplate("color", false, std::string_view("#fff")));
  // Present values apply, including strings for props that are not colors.
  CHECK(!shadowListNativeBindingKeepsTemplate("opacity", false));
  CHECK(!shadowListNativeBindingKeepsTemplate("uri", false, std::string_view("")));
  CHECK(!shadowListNativeBindingKeepsTemplate("nativeID", false, std::string_view("x")));
  // Hidden and visible always apply, and null counts as false.
  CHECK(!shadowListNativeBindingKeepsTemplate("hidden", true));
  CHECK(!shadowListNativeBindingKeepsTemplate("visible", true));
}
