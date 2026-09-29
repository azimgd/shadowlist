#pragma once

#include <react/renderer/attributedstring/AttributedString.h>
#include <react/renderer/attributedstring/AttributedStringBox.h>
#include <react/renderer/attributedstring/ParagraphAttributes.h>
#include <react/renderer/attributedstring/TextAttributes.h>
#include <react/renderer/components/text/BaseParagraphComponentDescriptor.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/textlayoutmanager/TextLayoutManager.h>
#include <react/utils/ContextContainer.h>

#include <shadowlist-core/Element.hpp>
#include <shadowlist-core/host/ElementSizeSpec.hpp>

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

}

/*
 * Measures one spec into the row size the core should use.
 * Text wraps in the list width minus the spec's insets, with no height limit.
 */
inline azimgd::shadowlist::Size measureElementSizeSpec(
  const TextLayoutManager& textLayoutManager,
  const azimgd::shadowlist::ElementSizeSpec& spec,
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
