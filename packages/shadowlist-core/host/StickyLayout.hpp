#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>

namespace azimgd::shadowlist {

/*
 * Pinning of the list header and footer, and of the section header overlay. Everything is
 * along the scroll axis and in the host's own units, points or pixels.
 */

/*
 * How far each auto hide bar has slid away, and the offset of the previous pin. Kept by the
 * host across frames.
 */
struct StickyState {
  double headerHidden = 0.0;
  double footerHidden = 0.0;
  double previousOffset = 0.0;
};

struct StickyInput {
  double offset = 0.0;
  double windowSize = 0.0;
  double contentSize = 0.0;

  bool hasHeader = false;
  double headerSize = 0.0;
  bool stickyHeader = false;
  bool autoHideHeader = false;

  bool hasFooter = false;
  double footerSize = 0.0;

  /*
   * Where the footer sits in the content without any translation.
   */
  double footerStart = 0.0;
  bool stickyFooter = false;
  bool autoHideFooter = false;

  /*
   * Only real user scrolls slide the auto hide bars. Other moves just reset the start point.
   */
  bool accumulate = false;
};

struct StickyTranslations {
  double header = 0.0;
  double footer = 0.0;
};

/*
 * Translations that pin the header to the top and the footer to the bottom of the viewport.
 */
StickyTranslations stickyTranslations(const StickyInput& input, StickyState& state);

/*
 * Position of the pinned section header among count headers, the last one starting at or
 * above the offset, or UNDEFINED_INDEX. leadingAt gives a header's leading edge by position and must not
 * decrease. Headers not placed yet can report infinity. Binary search.
 */
template <typename LeadingAt>
std::size_t pinnedSectionPosition(std::size_t count, double offset, LeadingAt leadingAt) {
  std::size_t found = UNDEFINED_INDEX;
  std::size_t low = 0;
  std::size_t high = count;
  while (low < high) {
    std::size_t mid = low + (high - low) / 2;
    if (leadingAt(mid) <= offset) {
      found = mid;
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return found;
}

/*
 * Where the pinned header's leading edge goes: at the offset, never above its own place, and
 * pushed up by the next header when there is one.
 */
inline double pinnedSectionLeading(double leading, double extent, double offset, bool hasNext, double nextLeading) {
  double pinned = leading > offset ? leading : offset;
  if (hasNext && nextLeading - extent < pinned) {
    pinned = nextLeading - extent;
  }
  return pinned;
}

struct SectionOverlayPosition {
  bool visible = false;
  double translation = 0.0;
};

/*
 * Where the section header overlay goes, given the section headers' offsets and sizes in order.
 * The active header is the last at or above the top, and the next one pushes it up.
 */
SectionOverlayPosition sectionOverlayPosition(
  const double* offsets,
  const double* sizes,
  std::size_t count,
  double offset);

}
