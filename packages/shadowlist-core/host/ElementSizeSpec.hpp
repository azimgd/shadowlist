#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

namespace azimgd::shadowlist {

/*
 * Describes a row's text before it renders. A host predicts its height with its own
 * text layout. Comes from the elementsSizeSpecs prop, a JSON array of these.
 */
struct ElementSizeSpec {
  static constexpr double DEFAULT_FONT_SIZE = 14.0;

  std::string key;
  std::string text;

  std::string fontFamily;
  double fontSize = DEFAULT_FONT_SIZE;
  std::string fontWeight;
  std::string fontStyle;
  double lineHeight = std::numeric_limits<double>::quiet_NaN();
  double letterSpacing = std::numeric_limits<double>::quiet_NaN();
  int numberOfLines = 0;

  /*
   * Space in the row around the text, in points, like padding or an avatar column. Width is
   * taken off the wrap width and height is added to the measured text.
   */
  double insetWidth = 0.0;
  double insetHeight = 0.0;

  /*
   * Share of the list width the text may use before insetWidth, like a chat bubble capped at
   * 75 percent.
   */
  double widthFraction = 1.0;

  // A known row height. When set, the text is not measured and this height is used as is.
  double fixedHeight = std::numeric_limits<double>::quiet_NaN();
};

/*
 * Feeds predicted sizes to the core a few rows per commit. A long list never spikes the
 * commit thread. The parsed specs are cached until the source or the width changes.
 */
class SizeSpecQueue final {
public:
  // Text layouts per run. Small enough for a frame, big enough to finish a window in a couple.
  static constexpr std::size_t BUDGET_PER_RUN = 24;

  /*
   * Measure the next specs of source. parse() returns them and only runs for a new source or
   * width, and measure(spec, width) returns the row size. A new width drops every prediction.
   */
  template <typename Parse, typename Measure>
  void run(Container& core, const std::shared_ptr<const void>& source, double width, Parse&& parse, Measure&& measure) {
    if (!(width > 0.0)) {
      return;
    }
    /*
     * Compare with the width the specs were measured at, not the core's window width. The
     * layout pass can move the core to the new width before this runs, and the stale
     * predictions would then never be dropped.
     */
    bool widthChanged = width_ != width;
    if (widthChanged) {
      Virtualizer::invalidatePredictions(&core);
      width_ = width;
    }
    bool sameSpecs = source_ == source && !widthChanged;
    if (sameSpecs && done_) {
      return;
    }
    if (!sameSpecs) {
      specs_ = parse();
      cursor_ = 0;
      source_ = source;
    }
    std::size_t measured = 0;
    while (cursor_ < specs_.size() && measured < BUDGET_PER_RUN) {
      const ElementSizeSpec& spec = specs_[cursor_];
      core.setPredictedSize(spec.key, measure(spec, width));
      ++cursor_;
      ++measured;
    }
    done_ = cursor_ >= specs_.size();
  }

  /*
   * Whether every spec of this source is measured.
   */
  bool finished(const std::shared_ptr<const void>& source) const {
    return done_ && source_ == source;
  }

private:
  std::shared_ptr<const void> source_;
  std::vector<ElementSizeSpec> specs_;
  std::size_t cursor_ = 0;
  bool done_ = false;
  // Width the current specs were measured at, 0 before the first run.
  double width_ = 0.0;
};

}
