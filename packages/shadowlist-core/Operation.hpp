#pragma once

#include <cstdint>
#include <string>

namespace azimgd::shadowlist {

/*
 * What an anchor pins to. Element pins to a specific row by key; EndEdge pins to the content
 * end (the inverted bottom pin and scrollToEnd) and ignores Anchor::key.
 */
enum class AnchorMode {
  Element,
  EndEdge,
};

/*
 * Scroll position in content space: row `key`'s leading edge sits `subOffset` px past the
 * viewport start. Unlike a pixel offset it survives key-set changes (prepend, insert,
 * remove), so holding it steady across a reconcile keeps the same content on screen.
 */
struct Anchor {
  std::string key = "";
  double subOffset = 0.0;
  AnchorMode mode = AnchorMode::Element;
};

/*
 * Live scroll-gesture phase, reported by the host so we don't have to guess user-vs-
 * programmatic from pixel proximity. Idle covers programmatic moves and their echoes;
 * Dragging and Settling mean a human is driving.
 */
enum class ScrollPhase {
  Idle,
  Dragging,
  Settling,
};

/*
 * Why the core wants to move the scroll offset. At most one is in flight at a time,
 * carried by the active Operation.
 */
enum class OperationType {
  MaintainAnchor,  // MVCP: keep the captured anchor element fixed across a reconcile
  ScrollToKey,     // scrollToIndex, resolved to a key once at request time
  ScrollToStart,   // scrollToStart: the leading row held at its offset below the viewport start
  ScrollToEnd,     // converge on the true bottom as off-screen rows are measured
  BottomPin,       // inverted list initial bottom pin (one-shot; dies on a user drag)
  ShrinkClamp,     // content shrank below the offset: pull back to the new max
};

/*
 * One in-flight offset correction. `id` doubles as the commit token: assigned when the
 * correction starts, kept while it stays in flight across the multi-frame settle (even as
 * the anchor retargets), and stamped on every offset write it produces. The host echoes
 * the token back on its next scroll report, so we recognise our own writes exactly instead
 * of guessing by pixel distance. A new intent (different type, or different anchored key)
 * gets a fresh id.
 */
struct Operation {
  std::uint64_t id = 0;
  OperationType type = OperationType::MaintainAnchor;
  Anchor target = {};

  /*
   * ScrollToKey only: where in the viewport the target row rests, as a fraction of the free
   * space around it (0 start, 0.5 centre, 1 end). Kept here rather than as a pixel
   * sub-offset so it can be rederived per frame (see resolveAnchorSubOffset).
   */
  double viewPosition = 0.0;
};

}
