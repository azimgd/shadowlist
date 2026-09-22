#pragma once

#include <cstdint>
#include <string>

namespace azimgd::shadowlist {

/*
 * What an anchor holds on to. Element holds a row by key. EndEdge holds the end of the
 * content for the inverted bottom pin and scrollToEnd, and ignores the key.
 */
enum class AnchorMode {
  Element,
  EndEdge,
};

/*
 * A scroll position given as a row and how far its top sits past the top of the viewport.
 * Unlike a pixel offset it survives prepends, inserts and removes, so holding it keeps
 * the same content on screen.
 */
struct Anchor {
  std::string key = "";
  double subOffset = 0.0;
  AnchorMode mode = AnchorMode::Element;
};

/*
 * The current scroll gesture as reported by the host, so we know whether the user is scrolling.
 * Idle covers our own moves. Dragging and Settling mean the user is in control.
 */
enum class ScrollPhase {
  Idle,
  Dragging,
  Settling,
};

/*
 * Why the core wants to move the scroll offset. Only one runs at a time.
 */
enum class OperationType {
  MaintainAnchor,  // Keep the visible content in place across a data update
  ScrollToKey,     // scrollToIndex, turned into a key when requested
  ScrollToStart,   // Hold the first row at its offset from the top
  ScrollToEnd,     // Keep closing in on the real bottom as rows get measured
  BottomPin,       // First bottom pin of an inverted list, dropped once the user drags
  ShrinkClamp,     // Content got shorter than the offset, so pull back to the new end
};

/*
 * One running offset correction. The id is stamped on every offset write it makes and stays
 * the same while it settles over several frames. The host sends it back with the next scroll
 * report, so we can tell our own writes apart. A new type or anchor key gets a new id.
 */
struct Operation {
  std::uint64_t id = 0;
  OperationType type = OperationType::MaintainAnchor;
  Anchor target = {};

  /*
   * For ScrollToKey, where the row should rest in the viewport, from 0 at the top to 1 at the bottom.
   * Kept as a fraction so the pixel offset can be worked out again each frame.
   */
  double viewPosition = 0.0;
};

}
