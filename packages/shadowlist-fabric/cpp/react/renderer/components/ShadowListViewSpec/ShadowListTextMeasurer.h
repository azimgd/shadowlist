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
 * One row's text, as the host can describe it before rendering: the string, the attributes
 * that affect how it wraps, and the fixed chrome around it.
 */
struct ShadowListMeasureSpec {
  std::string key;
  std::string text;

  std::string fontFamily;
  double fontSize = 14.0;
  std::string fontWeight;
  std::string fontStyle;
  double lineHeight = std::numeric_limits<double>::quiet_NaN();
  double letterSpacing = std::numeric_limits<double>::quiet_NaN();
  int maximumNumberOfLines = 0;

  /*
   * Everything in the row that is NOT the text, in points: padding, borders, an avatar
   * column, a timestamp row. The host knows these from its own styles; the core needs the
   * whole row's size, not the paragraph's.
   *
   * `insetWidth` is subtracted from the width the text gets to wrap in, and `insetHeight`
   * is added to the height it measures to. A row whose chrome does not reduce to two
   * scalars is one the host should leave unpredicted.
   */
  double insetWidth = 0.0;
  double insetHeight = 0.0;

  /*
   * Fraction of the list width the text may occupy before `insetWidth` is taken off it,
   * for the very common case of a percentage-width row element -- a chat bubble capped at
   * `maxWidth: '75%'`, say. A percentage cannot be expressed as a fixed inset, and rounding
   * it to one would mis-wrap every row at a width the author never tested.
   */
  double widthFraction = 1.0;

  /*
   * A height the host already knows exactly (a fixed-height row). When set, no text
   * measurement happens at all and this is published verbatim -- the cheapest possible
   * prediction, and the right one for rows that have no text to measure.
   */
  double fixedHeight = std::numeric_limits<double>::quiet_NaN();
};

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

namespace shadowlistTextMeasurerDetail {

inline FontWeight parseFontWeight(const std::string& value) {
  if (value == "100" || value == "ultralight") return FontWeight::UltraLight;
  if (value == "200" || value == "thin") return FontWeight::Thin;
  if (value == "300" || value == "light") return FontWeight::Light;
  if (value == "400" || value == "normal" || value == "regular") return FontWeight::Regular;
  if (value == "500" || value == "medium") return FontWeight::Medium;
  if (value == "600" || value == "semibold") return FontWeight::Semibold;
  if (value == "700" || value == "bold") return FontWeight::Bold;
  if (value == "800" || value == "heavy") return FontWeight::Heavy;
  if (value == "900" || value == "black") return FontWeight::Black;
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
 * Parse the `elementsMeasureSpecs` prop.
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
inline std::vector<ShadowListMeasureSpec> parseMeasureSpecs(const std::string& json) {
  std::vector<ShadowListMeasureSpec> specs;
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

    ShadowListMeasureSpec spec;
    spec.key = shadowlistTextMeasurerDetail::optionalString(entry, "key");
    if (spec.key.empty()) {
      continue;
    }

    spec.text = shadowlistTextMeasurerDetail::optionalString(entry, "text");
    spec.fontFamily = shadowlistTextMeasurerDetail::optionalString(entry, "fontFamily");
    spec.fontWeight = shadowlistTextMeasurerDetail::optionalString(entry, "fontWeight");
    spec.fontStyle = shadowlistTextMeasurerDetail::optionalString(entry, "fontStyle");
    spec.fontSize = shadowlistTextMeasurerDetail::optionalDouble(entry, "fontSize", 14.0);
    spec.lineHeight = shadowlistTextMeasurerDetail::optionalDouble(
      entry, "lineHeight", std::numeric_limits<double>::quiet_NaN());
    spec.letterSpacing = shadowlistTextMeasurerDetail::optionalDouble(
      entry, "letterSpacing", std::numeric_limits<double>::quiet_NaN());
    spec.maximumNumberOfLines = static_cast<int>(
      shadowlistTextMeasurerDetail::optionalDouble(entry, "maximumNumberOfLines", 0.0));
    spec.insetWidth = shadowlistTextMeasurerDetail::optionalDouble(entry, "insetWidth", 0.0);
    spec.insetHeight = shadowlistTextMeasurerDetail::optionalDouble(entry, "insetHeight", 0.0);
    spec.widthFraction = shadowlistTextMeasurerDetail::optionalDouble(entry, "widthFraction", 1.0);
    spec.fixedHeight = shadowlistTextMeasurerDetail::optionalDouble(
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
inline azimgd::shadowlist::Size measureSpec(
  const TextLayoutManager& textLayoutManager,
  const ShadowListMeasureSpec& spec,
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
    textAttributes.fontWeight = shadowlistTextMeasurerDetail::parseFontWeight(spec.fontWeight);
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
  paragraphAttributes.maximumNumberOfLines = spec.maximumNumberOfLines;

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
