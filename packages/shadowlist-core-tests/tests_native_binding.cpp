/*
 * ShadowListNative binding parsing (ShadowListNativeBinding.h): the expressions a template's
 * `bind` maps props to, and the color strings bound data may carry.
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
  // Empty segments are dropped rather than looked up.
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

TEST(nativeBindingParsesFormatStrings) {
  auto expression = parseShadowListNativeExpression("{name} · {stats.likes} likes");
  CHECK(expression.format);
  CHECK(!expression.negate);
  CHECK_EQ(expression.parts.size(), std::size_t{4});
  CHECK_EQ(expression.parts[0].path[0], std::string("name"));
  CHECK_EQ(expression.parts[1].text, std::string(" · "));
  CHECK_EQ(expression.parts[2].path.size(), std::size_t{2});
  CHECK_EQ(expression.parts[3].text, std::string(" likes"));

  // An unclosed brace stays literal text.
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
  // Missing/null values keep the template's static prop (a static `color` survives an unset `k`).
  CHECK(shadowListNativeBindingKeepsTemplate("color", true));
  CHECK(shadowListNativeBindingKeepsTemplate("borderColor", true));
  CHECK(shadowListNativeBindingKeepsTemplate("opacity", true));
  CHECK(shadowListNativeBindingKeepsTemplate("uri", true));
  // So do color strings that do not parse, including ''.
  CHECK(shadowListNativeBindingKeepsTemplate("color", false, std::string_view("")));
  CHECK(shadowListNativeBindingKeepsTemplate("backgroundColor", false, std::string_view("chartreuse")));
  CHECK(!shadowListNativeBindingKeepsTemplate("color", false, std::string_view("#fff")));
  // Values that are present apply, strings included for non-color props.
  CHECK(!shadowListNativeBindingKeepsTemplate("opacity", false));
  CHECK(!shadowListNativeBindingKeepsTemplate("uri", false, std::string_view("")));
  CHECK(!shadowListNativeBindingKeepsTemplate("nativeID", false, std::string_view("x")));
  // hidden/visible always apply: null is falsy.
  CHECK(!shadowListNativeBindingKeepsTemplate("hidden", true));
  CHECK(!shadowListNativeBindingKeepsTemplate("visible", true));
}
