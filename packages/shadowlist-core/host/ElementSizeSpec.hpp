#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Describes a row's text before it renders, so a host can predict its height with its own
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
   * Space in the row around the text, in points, like padding or an avatar column.
   * Width is taken off the wrap width and height is added to the measured text.
   * Rows whose extra space can't be described by these two numbers should not be predicted.
   */
  double insetWidth = 0.0;
  double insetHeight = 0.0;

  /*
   * Share of the list width the text may use before insetWidth, for rows with a percent width
   * such as a chat bubble capped at 75 percent. A fixed inset can't express that without wrapping wrong.
   */
  double widthFraction = 1.0;

  // A known row height. When set, the text is not measured and this height is used as is.
  double fixedHeight = std::numeric_limits<double>::quiet_NaN();
};

/*
 * Feeds predicted sizes to the core a few rows per commit.
 *
 * Measuring costs a text layout per row, about a tenth of a millisecond each, and doing a
 * whole list at once spikes the commit thread. So each run stops at the budget and the next
 * picks up where it stopped. A row not measured yet just uses the normal estimate.
 * The parsed specs are cached too, since parsing them on every commit would cost more than
 * the measuring. A new source, which the host keeps alive so its address can't be reused, or
 * a new width starts over.
 */
class SizeSpecQueue final {
public:
  // Text layouts per run. Small enough for a frame, big enough to finish a window in a couple.
  static constexpr std::size_t BUDGET_PER_RUN = 24;

  /*
   * Measure the next specs of source. parse() returns them and only runs for a new source or
   * width. measure(spec, width) returns the row size. A new width
   * makes every predicted height wrong, so they are all dropped first. Text wraps to the
   * list width, so nothing happens at zero width.
   */
  template <typename Parse, typename Measure>
  void run(Container& core, const std::shared_ptr<const void>& source, double width, Parse&& parse, Measure&& measure) {
    if (!(width > 0.0)) {
      return;
    }
    bool widthChanged = core.revision.windowContainerWidth != width;
    if (widthChanged) {
      Virtualizer::invalidatePredictions(&core);
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
   * Whether every spec of this source is measured. Until then the host needs more commits.
   */
  bool finished(const std::shared_ptr<const void>& source) const {
    return done_ && source_ == source;
  }

private:
  std::shared_ptr<const void> source_;
  std::vector<ElementSizeSpec> specs_;
  std::size_t cursor_ = 0;
  bool done_ = false;
};

}
