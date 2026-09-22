#pragma once

#include <folly/dynamic.h>
#include <folly/json.h>
#include <react/renderer/attributedstring/AttributedString.h>
#include <react/renderer/attributedstring/AttributedStringBox.h>
#include <react/renderer/attributedstring/ParagraphAttributes.h>
#include <react/renderer/attributedstring/TextAttributes.h>
#include <react/renderer/components/text/BaseParagraphComponentDescriptor.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/textlayoutmanager/TextLayoutManager.h>
#include <react/utils/ContextContainer.h>

#include <shadowlist-core/Element.hpp>

#include "ShadowListElementSizeSpec.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace facebook::react {

/*
 * Measures row text before the row renders, so its height is right from the first frame
 * instead of being fixed up while the user scrolls.
 * TextLayoutManager caches by text and text style only, not by node. So the real paragraph
 * hits the same cache entry later and the text is still measured once.
 * That only works while we share RN's TextLayoutManager. If we don't, the size is still
 * right and the row just measures twice.
 * Text with inline images or views can't be predicted. Skip those rows and the core uses
 * its usual estimate.
 */

/*
 * Finds a TextLayoutManager to share with RN's paragraph descriptor so both use one cache.
 * RN never stores its own and builds a new one on each lookup, so we store ours under RN's key.
 * This runs from a descriptor constructor so later descriptors pick it up. If RN's paragraph
 * descriptor was built first, text is measured twice but the size is still right.
 */
inline std::shared_ptr<const TextLayoutManager> getSharedTextLayoutManager(
  const std::shared_ptr<const ContextContainer>& contextContainer) {
  if (!contextContainer) {
    return nullptr;
  }

  if (auto existing = contextContainer->find<std::shared_ptr<TextLayoutManager>>(TextLayoutManagerKey);
      existing.has_value()) {
    return existing.value();
  }

  auto textLayoutManager = std::make_shared<TextLayoutManager>(contextContainer);
  contextContainer->insert(TextLayoutManagerKey, textLayoutManager);
  return textLayoutManager;
}

namespace shadowlist::detail {

inline FontWeight parseFontWeight(const std::string& value) {
  if (value == "100" || value == "ultralight") {
    return FontWeight::UltraLight;
  }
  if (value == "200" || value == "thin") {
    return FontWeight::Thin;
  }
  if (value == "300" || value == "light") {
    return FontWeight::Light;
  }
  if (value == "400" || value == "normal" || value == "regular") {
    return FontWeight::Regular;
  }
  if (value == "500" || value == "medium") {
    return FontWeight::Medium;
  }
  if (value == "600" || value == "semibold") {
    return FontWeight::Semibold;
  }
  if (value == "700" || value == "bold") {
    return FontWeight::Bold;
  }
  if (value == "800" || value == "heavy") {
    return FontWeight::Heavy;
  }
  if (value == "900" || value == "black") {
    return FontWeight::Black;
  }
  return FontWeight::Regular;
}

inline double optionalDouble(const folly::dynamic& object, const char* name, double fallback) {
  auto* value = object.get_ptr(name);
  if (value == nullptr || !value->isNumber()) {
    return fallback;
  }
  return value->asDouble();
}

inline std::string optionalString(const folly::dynamic& object, const char* name) {
  auto* value = object.get_ptr(name);
  if (value == nullptr || !value->isString()) {
    return std::string{};
  }
  return value->asString();
}

}

/*
 * Parses the elementsSizeSpecs prop. It is a JSON string so its shape can grow without
 * changing the native spec. Bad input just gives fewer specs and those rows fall back to
 * estimates, never a thrown error.
 */
inline std::vector<ShadowListElementSizeSpec> parseElementSizeSpecs(const std::string& json) {
  std::vector<ShadowListElementSizeSpec> specs;
  if (json.empty()) {
    return specs;
  }

  folly::dynamic parsed;
  try {
    parsed = folly::parseJson(json);
  } catch (const std::exception&) {
    return specs;
  }

  if (!parsed.isArray()) {
    return specs;
  }

  specs.reserve(parsed.size());
  for (const auto& entry : parsed) {
    if (!entry.isObject()) {
      continue;
    }

    ShadowListElementSizeSpec spec;
    spec.key = shadowlist::detail::optionalString(entry, "key");
    if (spec.key.empty()) {
      continue;
    }

    spec.text = shadowlist::detail::optionalString(entry, "text");
    spec.fontFamily = shadowlist::detail::optionalString(entry, "fontFamily");
    spec.fontWeight = shadowlist::detail::optionalString(entry, "fontWeight");
    // fontWeight can also be a number like 700, so turn it into a string.
    if (auto* fontWeight = entry.get_ptr("fontWeight"); fontWeight != nullptr && fontWeight->isNumber()) {
      spec.fontWeight = std::to_string(static_cast<int>(fontWeight->asDouble()));
    }
    spec.fontStyle = shadowlist::detail::optionalString(entry, "fontStyle");
    spec.fontSize = shadowlist::detail::optionalDouble(entry, "fontSize", ShadowListElementSizeSpec::DEFAULT_FONT_SIZE);
    spec.lineHeight = shadowlist::detail::optionalDouble(
      entry, "lineHeight", std::numeric_limits<double>::quiet_NaN());
    spec.letterSpacing = shadowlist::detail::optionalDouble(
      entry, "letterSpacing", std::numeric_limits<double>::quiet_NaN());
    spec.numberOfLines = static_cast<int>(
      shadowlist::detail::optionalDouble(entry, "numberOfLines", 0.0));
    spec.insetWidth = shadowlist::detail::optionalDouble(entry, "insetWidth", 0.0);
    spec.insetHeight = shadowlist::detail::optionalDouble(entry, "insetHeight", 0.0);
    spec.widthFraction = shadowlist::detail::optionalDouble(entry, "widthFraction", 1.0);
    spec.fixedHeight = shadowlist::detail::optionalDouble(
      entry, "fixedHeight", std::numeric_limits<double>::quiet_NaN());

    specs.push_back(std::move(spec));
  }

  return specs;
}

/*
 * Measures one spec into the row size the core should use.
 * Text wraps in the list width minus the spec's insets, with no height limit.
 */
inline azimgd::shadowlist::Size measureElementSizeSpec(
  const TextLayoutManager& textLayoutManager,
  const ShadowListElementSizeSpec& spec,
  double availableWidth,
  Float pointScaleFactor,
  SurfaceId surfaceId) {
  double rowWidth = availableWidth;

  if (!std::isnan(spec.fixedHeight)) {
    return {rowWidth, spec.fixedHeight};
  }

  double widthFraction = spec.widthFraction > 0.0 && spec.widthFraction <= 1.0 ? spec.widthFraction : 1.0;
  double textWidth = availableWidth * widthFraction - spec.insetWidth;
  if (textWidth <= 0.0) {
    return {rowWidth, spec.insetHeight};
  }

  TextAttributes textAttributes;
  textAttributes.fontSize = static_cast<Float>(spec.fontSize);
  if (!spec.fontFamily.empty()) {
    textAttributes.fontFamily = spec.fontFamily;
  }
  if (!spec.fontWeight.empty()) {
    textAttributes.fontWeight = shadowlist::detail::parseFontWeight(spec.fontWeight);
  }
  if (spec.fontStyle == "italic") {
    textAttributes.fontStyle = FontStyle::Italic;
  }
  if (!std::isnan(spec.lineHeight)) {
    textAttributes.lineHeight = static_cast<Float>(spec.lineHeight);
  }
  if (!std::isnan(spec.letterSpacing)) {
    textAttributes.letterSpacing = static_cast<Float>(spec.letterSpacing);
  }

  AttributedString attributedString;
  attributedString.setBaseTextAttributes(textAttributes);

  AttributedString::Fragment fragment;
  fragment.string = spec.text;
  fragment.textAttributes = textAttributes;
  /*
   * Leave parentShadowView empty on purpose. The cache key ignores it, which lets this
   * string share a cache entry with the real paragraph.
   */
  attributedString.appendFragment(std::move(fragment));

  ParagraphAttributes paragraphAttributes;
  paragraphAttributes.maximumNumberOfLines = spec.numberOfLines;

  LayoutConstraints layoutConstraints;
  layoutConstraints.minimumSize = {0, 0};
  layoutConstraints.maximumSize = {
    static_cast<Float>(textWidth),
    std::numeric_limits<Float>::infinity()};

  TextLayoutContext textLayoutContext;
  textLayoutContext.pointScaleFactor = pointScaleFactor;
  textLayoutContext.surfaceId = surfaceId;

  auto measurement = textLayoutManager.measure(
    AttributedStringBox{attributedString},
    paragraphAttributes,
    textLayoutContext,
    layoutConstraints);

  return {rowWidth, static_cast<double>(measurement.size.height) + spec.insetHeight};
}

}
