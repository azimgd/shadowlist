#pragma once

#include <cstddef>

namespace azimgd::shadowlist {

/*
 * Pinning of the list header and footer, and of the section header overlay. Everything is
 * along the scroll axis and in the host's own units, points or pixels.
 */

/*
 * How far each auto hide bar has slid away, and the offset of the last pin, so the next one
 * knows how far the user scrolled. Kept by the host across frames.
 */
struct StickyState {
  double headerHidden = 0.0;
  double footerHidden = 0.0;
  double lastOffset = 0.0;
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
  // Where the footer sits in the content without any translation.
  double footerStart = 0.0;
  bool stickyFooter = false;
  bool autoHideFooter = false;

  // Only real user scrolls slide the auto hide bars. Other moves just reset the start point.
  bool accumulate = false;
};

struct StickyTranslations {
  double header = 0.0;
  double footer = 0.0;
};

/*
 * Translations that pin the header to the top and the footer to the bottom of the viewport.
 * A sticky header gets pushed off by the footer or the content end, and a sticky footer never
 * rides up into the header in a short list.
 */
StickyTranslations stickyTranslations(const StickyInput& input, StickyState& state);

struct SectionOverlayPosition {
  bool visible = false;
  double translation = 0.0;
};

/*
 * Where the section header overlay goes. Offsets are the section headers' positions in order
 * and sizes their sizes. The active header is the last at or above the top, and the next one
 * pushes it up. Hidden when no header is at or above the top.
 */
SectionOverlayPosition sectionOverlayPosition(const double* offsets, const double* sizes, std::size_t count, double offset);

}
