#include <shadowlist-core/host/StickyLayout.hpp>

#include <algorithm>

namespace azimgd::shadowlist {

StickyTranslations stickyTranslations(const StickyInput& input, StickyState& state) {
  StickyTranslations result;
  double offset = input.offset;
  double headerSize = input.hasHeader ? input.headerSize : 0.0;
  double footerSize = input.hasFooter ? input.footerSize : 0.0;
  double autoHideDelta = input.accumulate ? offset - state.lastOffset : 0.0;
  state.lastOffset = offset;

  if (input.hasHeader) {
    if (input.autoHideHeader) {
      // Pin to the top, and slide it away once the list scrolls past its height.
      if (offset <= headerSize) {
        state.headerHidden = 0.0;
      } else {
        state.headerHidden = std::max(0.0, std::min(state.headerHidden + autoHideDelta, headerSize));
      }
      result.header = offset - state.headerHidden;
    } else if (input.stickyHeader) {
      // Pin to the top, but let the end of the content or the footer push it off.
      result.header = std::min(offset, input.contentSize - footerSize - headerSize);
    }
  }

  if (input.hasFooter) {
    double resting = offset + input.windowSize - footerSize - input.footerStart;
    if (input.autoHideFooter) {
      // Pin to the bottom, and slide it away unless we are near the end.
      double maxOffset = std::max(0.0, input.contentSize - input.windowSize);
      if (offset >= maxOffset - footerSize) {
        state.footerHidden = 0.0;
      } else {
        state.footerHidden = std::max(0.0, std::min(state.footerHidden + autoHideDelta, footerSize));
      }
      result.footer = resting + state.footerHidden;
    } else if (input.stickyFooter) {
      // Pin to the bottom, but in a short list never ride up into the header's space.
      result.footer = std::max(resting, headerSize - input.footerStart);
    }
  }
  return result;
}

SectionOverlayPosition sectionOverlayPosition(const double* offsets, const double* sizes, std::size_t count, double offset) {
  SectionOverlayPosition result;
  offset = std::max(offset, 0.0);
  bool hasActive = false;
  double activeSize = 0.0;
  bool hasNext = false;
  double nextOffset = 0.0;
  for (std::size_t index = 0; index < count; ++index) {
    if (offsets[index] <= offset) {
      hasActive = true;
      activeSize = sizes[index];
    } else {
      hasNext = true;
      nextOffset = offsets[index];
      break;
    }
  }
  if (!hasActive) {
    return result;
  }
  result.visible = true;
  result.translation = hasNext ? std::min(offset, nextOffset - activeSize) : offset;
  return result;
}

}
