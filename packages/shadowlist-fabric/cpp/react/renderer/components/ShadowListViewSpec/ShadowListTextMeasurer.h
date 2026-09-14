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
 * AHEAD-OF-TIME ROW MEASUREMENT
 *
 * A row's height is normally discovered the expensive way: JS renders it, Fabric commits
 * it, Yoga lays it out, and only then does the core learn that its estimate was wrong and
 * reflow every row after it -- possibly moving the scroll offset to keep the anchor pinned.
 * On a variable-height list that correction runs continuously for the whole scroll.
 *
 * The height was knowable all along. `TextLayoutManager::measure` is a pure function of
 * (AttributedString, ParagraphAttributes, LayoutConstraints): it takes no shadow node, no
 * view, and no JS, so the same number can be computed for a row that does not exist yet.
 * Feeding it to the core as a prediction (Container::predictedSizes) means geometry is
 * correct from the first frame instead of converging over the scroll.
 *
 * WHY THIS IS NOT A SECOND MEASUREMENT
 *
 * TextLayoutManager caches results, and -- this is the part that makes the whole scheme pay
 * for itself -- the cache key deliberately ignores node identity. TextMeasureCacheKey's
 * equality and hash go through `areAttributedStringsEquivalentLayoutWise` /
 * `attributedStringHashLayoutWise`, which look only at each fragment's string and its
 * layout-affecting text attributes. `parentShadowView.tag` is not part of the key.
 *
 * So an AttributedString built here from scratch, with no shadow node behind it, hits the
 * very same cache entry that the real ParagraphShadowNode will ask for when the row is
 * finally rendered. The measurement happens once; we simply move it off the commit critical
 * path and onto the frame that had the information first.
 *
 * That only holds while we and RN share one TextLayoutManager instance -- see
 * getSharedTextLayoutManager below for why that is best-effort rather than guaranteed, and
 * why a miss costs performance rather than correctness.
 *
 * WHAT CANNOT BE PREDICTED
 *
 * Inline attachments (an image or view embedded in text) are the documented exception: their
 * fragments compare on `parentShadowView.layoutMetrics`, which does not exist before layout.
 * A spec is skipped rather than guessed at when the row cannot be described by text alone,
 * and the core falls back to its ordinary estimate for that row -- predicted and unpredicted
 * rows coexist by design.
 */

/*
 * Resolve the TextLayoutManager to measure through, preferring the one RN's own Paragraph
 * descriptor uses so our measurements and its share a cache.
 *
 * RN looks its instance up with `getManagerByName<TextLayoutManager>(contextContainer,
 * TextLayoutManagerKey)`, which returns whatever is stored under that key and otherwise
 * CONSTRUCTS A FRESH ONE -- and nothing in React Native ever stores it. So by default every
 * descriptor that asks ends up with a private instance and a private cache.
 *
 * Publishing ours under the same key fixes that for every descriptor constructed after this
 * point, which is why this runs from a descriptor constructor rather than lazily at measure
 * time. Whether we win the race against ParagraphComponentDescriptor's own construction is
 * not guaranteed and deliberately not depended on: if we lose, our prediction is still the
 * correct size and the row's real measurement simply misses the cache and computes it a
 * second time. The cost of losing is one text layout, not a wrong row.
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
 * Parse the `elementsSizeSpecs` prop.
 *
 * The prop is a JSON string rather than a codegen'd array of objects for one reason: the
 * whole point of the feature is to describe rows the host has NOT rendered, so the shape is
 * open-ended and host-specific, and a schema baked into the native spec would have to be
 * revised every time somebody's row layout gained a field. Parsing is guarded by a props
 * pointer comparison at the call site, so an unchanged window costs nothing.
 *
 * Malformed input yields fewer specs, never an exception: a prediction is an optimization,
 * and a host that ships a bad spec should get an unpredicted (estimated) row, not a list
 * that fails to commit.
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
    // TextStyle['fontWeight'] also admits numbers (700); normalize them to the string form.
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
 * Measure one spec into the size the core should carry for that row.
 *
 * `availableWidth` is the list's own width: rows fill it, so that is the constraint the
 * text wraps under, minus whatever chrome the spec declares. The height is unconstrained --
 * measuring is exactly the question "how tall does this become".
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
   * `parentShadowView` is left default-constructed on purpose. It exists so the mounting
   * layer can find the node a fragment came from, and it is excluded from the measure
   * cache key for non-attachment fragments -- which is precisely what lets this
   * node-less AttributedString share a cache entry with the real one.
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
