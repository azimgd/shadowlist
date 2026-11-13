#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/VirtualizerInternal.hpp>

#include <cmath>

namespace azimgd::shadowlist {

namespace {
/*
 * How many frames an operation that arrived on the end of a grid waits for the rows to settle.
 */
constexpr int GRID_END_WAIT_FRAMES = 3;

/*
 * Turn the running operation's target into an unclamped offset for this frame.
 * An end edge target is maxOffset. A row target is the row's offset plus its sub offset,
 * worked out each frame so it follows the row while nearby rows get measured.
 * Returns false when the row's key is gone. Header size changes are handled elsewhere.
 */
bool resolveAnchorOffset(Container& container, const Operation& operation, double maxOffset, double& outOffset) {
  if (operation.target.mode != AnchorMode::Row) {
    outOffset = maxOffset;
    return true;
  }
  std::size_t anchorIndex = container.indexOfKey(operation.target.key);
  if (anchorIndex == UNDEFINED_INDEX) {
    return false;
  }
  outOffset = container.getRowOffset(anchorIndex) + resolveAnchorOffset(container, operation, anchorIndex);
  return true;
}

/*
 * The sizes and offsets resolveScroll reads, taken once at the start of the frame.
 */
struct ScrollFrame {
  std::size_t rowCount = 0;
  double windowSize = 0.0;
  double totalSize = 0.0;
  double currentOffset = 0.0;
  double maxOffset = 0.0;
  double previousTotalForScrollToEnd = 0.0;
};

/*
 * What a step of resolveScroll decided. Continue runs the next step, the others return.
 */
enum class ScrollStep { Continue, Done, Corrected };

double clampOffset(double offset, double maxOffset) {
  if (offset < 0.0) {
    offset = 0.0;
  }
  if (offset > maxOffset) {
    offset = maxOffset;
  }
  return offset;
}

/*
 * Reuse the id when the same correction is requested again. The host's reply then still
 * matches it over several frames. A new type or a new row gets a new id, and the wait on the
 * end of a grid starts over with it.
 */
std::uint64_t operationId(Container& container, OperationType type, AnchorMode mode, const std::string& key) {
  if (container.state.operation && container.state.operation->type == type &&
      container.state.operation->target.mode == mode &&
      (mode != AnchorMode::Row || container.state.operation->target.key == key)) {
    return container.state.operation->commitToken;
  }
  container.state.operationGeometryVersion = container.state.geometryVersion;
  container.state.operationEndWaitFrames = 0;
  return container.state.nextCommitToken++;
}

/*
 * Start or repeat a correction that aims at the end, until the view gets there.
 */
void requestFixed(Container& container, OperationType type) {
  container.state.clampedAnchor.reset();
  if (container.state.operation && container.state.operation->type != type) {
    SHADOWLIST_LOG("  op replaced: type=%d key=%s -> type=%d", static_cast<int>(container.state.operation->type),
      container.state.operation->target.key.c_str(), static_cast<int>(type));
  }
  container.state.operation =
    Operation{operationId(container, type, AnchorMode::EndEdge, ""), type, Anchor{"", 0.0, AnchorMode::EndEdge}};
}

/*
 * Start or repeat a correction that aims at a row. The target is looked up by key each
 * frame. It follows the row while nearby rows get measured.
 */
void requestAnchor(
  Container& container,
  OperationType type,
  const std::string& key,
  double delta,
  double viewPosition = 0.0,
  double viewOffset = 0.0) {
  container.state.clampedAnchor.reset();
  const std::optional<Operation>& running = container.state.operation;
  if (running && (running->type != type || running->target.key != key)) {
    SHADOWLIST_LOG("  op replaced: type=%d key=%s -> type=%d key=%s", static_cast<int>(running->type),
      running->target.key.c_str(), static_cast<int>(type), key.c_str());
  }
  container.state.operation =
    Operation{operationId(container, type, AnchorMode::Row, key), type, Anchor{key, delta, AnchorMode::Row},
      viewPosition, viewOffset};
}

/*
 * Step 0. Content shrank and left the view past the new end, like collapsing a tree.
 * Go back to the bottom. Checking for a shrink avoids fighting a bounce.
 * It runs as an operation so it survives recommits, and step 3 clears it on arrival.
 * The clamp aims at the row the reader was on when that row is still there. Step 3 clamps
 * the row's offset to the new end, which is the same pull back. The total can be short only
 * because the rows that moved in are still estimated. When the layout pass measures them
 * taller, a clamp aimed at the row goes back to it. One aimed at the end would follow the
 * end down and leave the row behind.
 * A clamp started during a gesture aims at the end. The host shifts a moving view by
 * the correction instead of writing it, and only an end target is reached that way.
 */
void requestShrinkClamp(
  Container& container,
  const ScrollFrame& frame,
  const std::string& anchorKey,
  std::size_t anchorIndex,
  double anchorDelta,
  bool hadRowsBefore,
  bool reportedBeforeWrite) {
  if (!reportedBeforeWrite && !container.inverted && frame.rowCount > 0 &&
      frame.currentOffset > frame.maxOffset + OFFSET_MOVED_THRESHOLD &&
      frame.totalSize < frame.previousTotalForScrollToEnd) {
    /*
     * Only when no anchor will place the view. The shrink is often above the viewport, and
     * the row on screen moved up with it. Steps 3 and 4 follow that row and clamp when
     * needed. Jumping to the end would show rows further down instead.
     */
    bool anchorSurvived = !container.state.operation && hadRowsBefore && !anchorKey.empty() &&
      anchorIndex != UNDEFINED_INDEX;
    bool anchorPlacesView = false;
    if (container.state.operation && container.state.operation->target.mode == AnchorMode::Row) {
      anchorPlacesView = container.indexOfKey(container.state.operation->target.key) != UNDEFINED_INDEX;
    } else if (anchorSurvived) {
      anchorPlacesView =
        std::fabs(container.getRowOffset(anchorIndex) + anchorDelta - frame.currentOffset) >= OFFSET_MOVED_THRESHOLD;
    }
    if (!anchorPlacesView) {
      if (anchorSurvived && !container.state.gestureActive) {
        requestAnchor(container, OperationType::ShrinkClamp, anchorKey, anchorDelta);
      } else {
        requestFixed(container, OperationType::ShrinkClamp);
      }
    }
  }
}

/*
 * Step 1. Scroll to an index. Returns true when the command starts this frame.
 */
bool requestScrollToRow(Container& container, const ScrollFrame& frame) {
  bool requested = false;
  if (container.state.scrollToRowTarget != UNDEFINED_INDEX) {
    if (container.state.scrollToRowTarget < frame.rowCount) {
      /*
       * Follow the row itself, not a fixed offset. Its offset is a guess at first, and
       * following the row settles on it as the area gets measured.
       */
      const std::string targetKey = container.getRowAtIndex(container.state.scrollToRowTarget).key;
      // The view position travels with the operation and is applied again each frame.
      requestAnchor(container, OperationType::ScrollToKey, targetKey, 0.0, container.state.scrollToRowViewPosition,
        container.state.scrollToRowViewOffset);
      requested = true;
      // This replaces the bottom pin, but only for an index in range.
      container.state.invertedInitialized = true;
      container.state.invertedOpeningPin = false;
    }
    container.state.scrollToRowTarget = UNDEFINED_INDEX;
  }
  return requested;
}

/*
 * Step 1b. Scroll to the start. It holds the first row below the header so it keeps up
 * with measuring. Momentum neither cancels nor moves it because it is a command.
 * Returns true when the command starts this frame.
 */
bool requestScrollToStart(Container& container, const ScrollFrame& frame) {
  bool requested = false;
  if (container.state.pendingScrollToStart) {
    container.state.pendingScrollToStart = false;
    if (frame.rowCount > 0) {
      std::size_t leading = 0;
      if (container.getRowOffset(frame.rowCount - 1) < container.getRowOffset(0)) {
        leading = frame.rowCount - 1;
      }
      requestAnchor(
        container,
        OperationType::ScrollToStart,
        container.getRowAtIndex(leading).key,
        -container.getRowOffset(leading));
      requested = true;
      container.state.invertedInitialized = true;
      container.state.invertedOpeningPin = false;
    }
  }
  return requested;
}

/*
 * Step 2. An inverted list sticks to the bottom until it gets there, following it as the
 * content grows. Wait for the window size so the target is right.
 */
void requestOpeningBottomPin(Container& container, const ScrollFrame& frame, bool offsetConfirmed) {
  if (container.inverted && !container.state.invertedInitialized && frame.rowCount > 0 && frame.windowSize > 0.0) {
    requestFixed(container, OperationType::BottomPin);
    container.state.invertedOpeningPin = true;
    /*
     * Done only once the list can scroll, sits at the bottom and the rows on screen there have
     * real sizes. Until then keep pinning. After a dataset swap the host can report an
     * offset that equals the bottom of the new rows' estimates by chance. That bottom moves
     * once they are measured.
     */
    if (offsetConfirmed &&
        frame.totalSize > frame.windowSize &&
        hasTrustedEnd(container) &&
        std::fabs(frame.currentOffset - frame.maxOffset) < OFFSET_ARRIVED_THRESHOLD) {
      container.state.invertedInitialized = true;
    }
  }
}

/*
 * Step 2b. Scroll to the end, aiming again every frame as the content grows. It is done
 * when the view is at the bottom, the total held still and the rows on screen there have real
 * sizes. A row measured after that would move the end with nothing left to follow it.
 * An inverted list then rests on its bottom again, also when the view was already there.
 * A drag cancels it, momentum does not.
 */
void requestScrollToEnd(Container& container, const ScrollFrame& frame) {
  if (container.state.pendingScrollToEnd && frame.rowCount > 0 && frame.windowSize > 0.0) {
    container.state.invertedOpeningPin = false;
    bool atBottom = std::fabs(frame.currentOffset - frame.maxOffset) < OFFSET_ARRIVED_THRESHOLD;
    bool totalStable = frame.totalSize == frame.previousTotalForScrollToEnd;
    if (atBottom && totalStable && hasTrustedEnd(container)) {
      container.state.pendingScrollToEnd = false;
      container.state.invertedBottomReleased = false;
    } else {
      requestFixed(container, OperationType::ScrollToEnd);
    }
  }
}

/*
 * Step 3. Keep driving the running operation until the view gets there. The target is
 * worked out again each frame. It ends when its row is gone or the view arrives.
 */
ScrollStep driveOperation(
  Container& container,
  const ScrollFrame& frame,
  bool offsetConfirmed,
  bool commandRequestedThisFrame) {
  if (container.state.operation) {
    double rawTarget = 0.0;
    if (!resolveAnchorOffset(container, *container.state.operation, frame.maxOffset, rawTarget)) {
      container.state.operation.reset();
    } else {
      double target = clampOffset(rawTarget, frame.maxOffset);
      /*
       * A host report at the target ends the operation. So does our own write, if the host
       * last reported the target too. Without that, a resting list only sees its own writes
       * and republishes on every commit, about 500 times for one streaming reply.
       */
      bool reportedAtTarget = std::fabs(container.state.previousReportedOffset - target) < OFFSET_ARRIVED_THRESHOLD;
      bool arrived = (offsetConfirmed || reportedAtTarget) &&
        std::fabs(frame.currentOffset - target) < OFFSET_ARRIVED_THRESHOLD;
      /*
       * Grid tracks move apart as rows get measured. An operation that rests on the end holds
       * no row at the top of the viewport, and once it is done the anchor holds one track only.
       * Rows measured after that would move the track and take the view off the end, or make
       * room to reach a target row the end cut short, with nothing left to aim at it. Keep the
       * operation while a row moved or changed size since its previous frame, for at most
       * GRID_END_WAIT_FRAMES frames. A row that resizes on every frame must not keep it forever.
       */
      bool geometryChanged = container.state.operationGeometryVersion != container.state.geometryVersion;
      container.state.operationGeometryVersion = container.state.geometryVersion;
      if (arrived && container.numberOfColumns > 1 && rawTarget >= frame.maxOffset && geometryChanged &&
          container.state.operationEndWaitFrames < GRID_END_WAIT_FRAMES) {
        ++container.state.operationEndWaitFrames;
        arrived = false;
      }
      /*
       * A hold that rests on the end is cut short by it, or holds the last rows of a list that
       * rests there. While a row in the window that ends the content has only a guessed size,
       * that end is a guess too. Keep the hold until those rows have their sizes. It follows the
       * end as they come in. Once it is done the anchor is the top row, and the same sizes would
       * push the held row off its place. A scroll command lands on the end it finds.
       */
      OperationType type = container.state.operation->type;
      bool holdsRow = container.state.operation->target.mode == AnchorMode::Row &&
        (type == OperationType::MaintainAnchor || type == OperationType::ShrinkClamp);
      if (arrived && holdsRow && rawTarget >= frame.maxOffset && !hasTrustedEnd(container)) {
        arrived = false;
      }
      if (arrived) {
        /*
         * A host can have mounted rows beyond the measured range. Their late sizes can make
         * room after the hold reaches a short end. Keep its target for size compensation,
         * without keeping a running operation waiting for rows the core did not ask to mount.
         */
        if (holdsRow && rawTarget > frame.maxOffset + OFFSET_MOVED_THRESHOLD) {
          container.state.clampedAnchor = container.state.operation->target;
        }
        SHADOWLIST_LOG("  op arrived: type=%d key=%s index=%zd target=%.1f",
          static_cast<int>(container.state.operation->type),
          container.state.operation->target.key.c_str(),
          static_cast<std::ptrdiff_t>(container.state.operation->target.mode == AnchorMode::Row
            ? container.indexOfKey(container.state.operation->target.key) : UNDEFINED_INDEX),
          target);
        /*
         * One offset holds one track of a grid. Hold the track of the row the operation landed
         * on, not the one the capture picked.
         */
        const std::string& landedKey = container.state.operation->target.key;
        if (container.numberOfColumns > 1 && container.state.operation->target.mode == AnchorMode::Row &&
            container.isAnchorable(landedKey)) {
          std::size_t landedIndex = container.indexOfKey(landedKey);
          container.state.anchor =
            Anchor{landedKey, frame.currentOffset - container.getRowOffset(landedIndex), AnchorMode::Row};
        }
        /*
         * Where a command lands an inverted list decides whether it rests on its bottom. Away
         * from the bottom it leaves it, the same as a drag there. Otherwise the list still counts
         * as resting on its newest row, and a scroll to the top of a newest row taller than the
         * window would be pulled down to the bottom by the next frame. On the bottom it rests
         * there again, also when the view was already there and the landing moved nothing.
         */
        OperationType arrivedType = container.state.operation->type;
        if (container.inverted &&
            (arrivedType == OperationType::ScrollToKey || arrivedType == OperationType::ScrollToStart)) {
          container.state.invertedBottomReleased = !atInvertedBottom(target, frame.totalSize, frame.windowSize);
        }
        container.state.operation.reset();
        /*
         * A command that is already where it wants to be is done. Holding the old anchor
         * would move the view to a row further down after a data swap.
         */
        if (commandRequestedThisFrame) {
          return ScrollStep::Done;
        }
      } else {
        // The offset moved. The caller picks the measured range again.
        correctOffset(container, target);
        return ScrollStep::Corrected;
      }
    }
  }
  return ScrollStep::Continue;
}

/*
 * Step 4. With no correction running, keep the anchor row where it was on screen.
 * Rows added or removed above start a correction. Plain scrolling does nothing.
 * Returns true if the offset moved.
 */
bool holdAnchor(
  Container& container,
  const ScrollFrame& frame,
  const std::string& anchorKey,
  std::size_t anchorIndex,
  double anchorDelta,
  bool hadRowsBefore) {
  if (hadRowsBefore && !anchorKey.empty()) {
    if (anchorIndex != UNDEFINED_INDEX) {
      /*
       * An inverted list resting on its newest content row holds the true bottom instead of
       * the row's old position. The row can grow after first paint, and holding its position
       * would leave a gap at the bottom and fight the bottom pin. Trailing decoration rows
       * don't count as the newest row.
       * Not when the user has scrolled up or a scroll command landed away from the bottom, or
       * the view gets pulled from under them. Not while a finger is down either, or the content
       * snaps back on every touch frame.
       * A list still settling on the bottom it opened at always holds the bottom, and so does
       * one with followAppends that rested there.
       */
      bool invertedBottomAnchor = false;
      if (container.inverted && !container.state.invertedBottomReleased && !container.state.gestureActive) {
        if (container.state.invertedOpeningPin && container.state.restingAtInvertedBottom) {
          invertedBottomAnchor = true;
        } else {
          invertedBottomAnchor = isLastAnchorable(container, anchorIndex) || followsBottom(container);
        }
      }
      double rawAnchoredOffset = invertedBottomAnchor
        ? frame.maxOffset
        : container.getRowOffset(anchorIndex) + anchorDelta;
      if (std::fabs(rawAnchoredOffset - frame.currentOffset) >= OFFSET_MOVED_THRESHOLD) {
        if (invertedBottomAnchor) {
          requestFixed(container, OperationType::BottomPin);
        } else {
          requestAnchor(container, OperationType::MaintainAnchor, anchorKey, anchorDelta);
        }
        correctOffset(container, clampOffset(rawAnchoredOffset, frame.maxOffset));
        return true;
      }
    }
  }
  return false;
}
}

std::vector<Anchor> Virtualizer::captureFallbackAnchors(
  Container& container,
  std::size_t anchorIndex,
  double inputOffset) {
  std::vector<Anchor> candidates;
  const std::vector<Row>& rows = container.revision.rows;
  if (anchorIndex == UNDEFINED_INDEX || container.state.anchor.mode != AnchorMode::Row) {
    return candidates;
  }

  /*
   * Collect the anchorable rows after the anchor that start inside the viewport, each with
   * the offset that holds it in place. In a grid, keep going until every column has passed
   * the bottom of the viewport.
   */
  double viewportEnd = inputOffset + container.getWindowSize();
  std::size_t columns = container.numberOfColumns > 0 ? container.numberOfColumns : 1;
  std::size_t tracksPast = 0;
  for (std::size_t index = anchorIndex + 1; index < rows.size() && tracksPast < columns; ++index) {
    double rowOffset = container.getRowOffset(index);
    if (rowOffset >= viewportEnd) {
      ++tracksPast;
      continue;
    }
    if (container.isAnchorable(rows[index].key)) {
      candidates.push_back(Anchor{rows[index].key, inputOffset - rowOffset, AnchorMode::Row});
    }
  }
  return candidates;
}

std::size_t Virtualizer::captureAnchor(Container& container, double inputOffset) {
  if (container.state.clampedAnchor &&
      (!container.isAnchorable(container.state.clampedAnchor->key) ||
       container.indexOfKey(container.state.clampedAnchor->key) == UNDEFINED_INDEX)) {
    container.state.clampedAnchor.reset();
  }
  Anchor& anchor = container.state.anchor;
  const std::vector<Row>& previousRows = container.revision.rows;
  if (previousRows.empty()) {
    anchor.key.clear();
    anchor.offset = 0.0;
    anchor.mode = AnchorMode::Row;
    return UNDEFINED_INDEX;
  }
  auto holdRow = [&](std::size_t index, double offset) {
    anchor.key = previousRows[index].key;
    anchor.offset = offset;
    anchor.mode = AnchorMode::Row;
    return index;
  };

  auto rowOffsetOf = [&](const Row& row) {
    return container.horizontal ? row.offsetX : row.offsetY;
  };
  auto rowSizeOf = [&](const Row& row) {
    return container.horizontal ? row.width : row.height;
  };

  /*
   * In a grid the tracks move independently as rows get measured. A new anchor each frame
   * lets the row the reader was on drift. Keep the previous anchor while it is still on screen.
   */
  if (container.numberOfColumns > 1 && container.isAnchorable(anchor.key)) {
    std::size_t previousAnchorIndex = container.indexOfKey(anchor.key);
    if (previousAnchorIndex != UNDEFINED_INDEX) {
      const Row& previousAnchor = previousRows[previousAnchorIndex];
      double start = rowOffsetOf(previousAnchor);
      if (start + rowSizeOf(previousAnchor) > inputOffset && start < inputOffset + container.getWindowSize()) {
        return holdRow(previousAnchorIndex, inputOffset - start);
      }
    }
  }

  /*
   * The anchor is the first content row at the top of the viewport. Decoration rows like
   * date pills are skipped. firstPast keeps the actual top row in case the viewport holds
   * only decoration.
   * A single column is in order. Binary search for the start. Grids scan from 0.
   */
  std::size_t scanStart = 0;
  if (container.numberOfColumns <= 1) {
    std::size_t low = 0;
    std::size_t high = previousRows.size();
    while (low < high) {
      std::size_t mid = low + (high - low) / 2;
      const Row& row = previousRows[mid];
      if (rowOffsetOf(row) + rowSizeOf(row) <= inputOffset) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    scanStart = low;
  }

  std::size_t firstPast = UNDEFINED_INDEX;
  for (std::size_t previousRowIndex = scanStart; previousRowIndex < previousRows.size(); ++previousRowIndex) {
    const Row& previousRow = previousRows[previousRowIndex];
    double rowOffset = rowOffsetOf(previousRow);
    double rowSize = rowSizeOf(previousRow);

    if (rowOffset + rowSize <= inputOffset) {
      continue;
    }
    if (firstPast == UNDEFINED_INDEX) {
      firstPast = previousRowIndex;
    }
    if (container.isAnchorable(previousRow.key)) {
      /*
       * A row with only a guessed size may change size in several steps, moving everything
       * below it each time. This is common right after a prepend at the top. If a row with a
       * known size starts inside the viewport, anchor that one instead.
       * Only for single column lists. An inverted list that may still hold its bottom uses
       * its anchor to tell whether it rests on the newest row, and keeps the top row. Once it
       * left the bottom or a gesture runs, nothing reads the anchor that way.
       * A list at its start keeps the top row too. No row sits above it there, and a row held
       * further down would leave the start when the rows above it get measured.
       * The row held so far comes first when it starts inside the viewport below a row with a
       * guessed size: the row of the running correction, or the previous anchor. After a prepend
       * whose rows are not measured yet that is the reader's row, and holding the top row would
       * let those rows push it down the screen.
       */
      bool anchorTellsNewestRow = container.inverted && !container.state.invertedBottomReleased &&
        !container.state.gestureActive;
      bool atStart = inputOffset < OFFSET_MOVED_THRESHOLD;
      std::size_t anchorIndex = previousRowIndex;
      if (container.numberOfColumns <= 1 && !anchorTellsNewestRow && !atStart) {
        const std::optional<Operation>& running = container.state.operation;
        const std::string& heldKey =
          running && running->target.mode == AnchorMode::Row ? running->target.key : anchor.key;
        bool topGuessed = !previousRow.measured && !previousRow.predicted;
        /*
         * Without duplicate keys a top row that is the row held so far is the held row alone,
         * and no row below it is. With a known size it stays the anchor and no other row is
         * read, which is how most frames of a scroll end. With a guessed size the first row
         * with a known size takes over and the rows after that one are not read.
         */
        bool topIsHeld = !container.revision.hasDuplicateKeys && heldKey == previousRow.key;
        bool guessedAbove = false;
        double viewportEnd = inputOffset + container.getWindowSize();
        std::size_t scanEnd = topIsHeld && !topGuessed ? previousRowIndex : previousRows.size();
        for (std::size_t laterIndex = previousRowIndex; laterIndex < scanEnd; ++laterIndex) {
          const Row& laterRow = previousRows[laterIndex];
          if (laterIndex > previousRowIndex && rowOffsetOf(laterRow) >= viewportEnd) {
            break;
          }
          bool known = laterRow.measured || laterRow.predicted;
          if (guessedAbove && container.isAnchorable(laterRow.key)) {
            if (laterRow.key == heldKey) {
              anchorIndex = laterIndex;
              break;
            }
            if (topGuessed && known && anchorIndex == previousRowIndex) {
              anchorIndex = laterIndex;
              if (topIsHeld) {
                break;
              }
            }
          }
          guessedAbove = guessedAbove || !known;
        }
      }
      return holdRow(anchorIndex, inputOffset - rowOffsetOf(previousRows[anchorIndex]));
    }
  }

  /*
   * No content row is in or below the viewport. Use the last content row, then the top row
   * even if it is decoration, then the very last row. There is always an anchor.
   */
  for (std::size_t reverse = previousRows.size(); reverse-- > 0;) {
    if (container.isAnchorable(previousRows[reverse].key)) {
      return holdRow(reverse, inputOffset - rowOffsetOf(previousRows[reverse]));
    }
  }
  std::size_t fallback = firstPast != UNDEFINED_INDEX ? firstPast : previousRows.size() - 1;
  return holdRow(fallback, inputOffset - rowOffsetOf(previousRows[fallback]));
}

bool Virtualizer::resolveScroll(
  Container& container,
  const std::string& anchorKey,
  std::size_t anchorIndex,
  double anchorDelta,
  bool hadRowsBefore,
  bool offsetConfirmed,
  bool reportedBeforeWrite) {
  std::size_t rowCount = container.revision.rows.size();
  container.state.offsetCorrected = false;

  // An emptied list starts over. An inverted list sticks to the bottom again when rows arrive.
  if (rowCount == 0) {
    container.state.invertedInitialized = false;
    container.state.invertedBottomReleased = false;
    container.state.invertedOpeningPin = false;
    container.state.operation.reset();
    container.state.pendingScrollToEnd = false;
  }

  ScrollFrame frame;
  frame.rowCount = rowCount;
  frame.windowSize = container.getWindowSize();
  frame.totalSize = container.horizontal ? container.revision.contentWidth : container.revision.contentHeight;
  frame.currentOffset = container.horizontal ? container.revision.offsetX : container.revision.offsetY;
  frame.maxOffset = frame.totalSize - frame.windowSize;
  if (frame.maxOffset < 0.0) {
    frame.maxOffset = 0.0;
  }

  // Track the total every frame so step 2b can tell when the bottom stops growing.
  frame.previousTotalForScrollToEnd = container.state.pendingScrollToEndPreviousTotal;
  container.state.pendingScrollToEndPreviousTotal = frame.totalSize;

  requestShrinkClamp(container, frame, anchorKey, anchorIndex, anchorDelta, hadRowsBefore, reportedBeforeWrite);

  /*
   * Set when a scroll command arrives this frame. The command owns the offset. Holding
   * the old visible content must not pull the view back.
   */
  bool commandRequestedThisFrame = requestScrollToRow(container, frame);
  if (requestScrollToStart(container, frame)) {
    commandRequestedThisFrame = true;
  }

  requestOpeningBottomPin(container, frame, offsetConfirmed);
  requestScrollToEnd(container, frame);

  ScrollStep step = driveOperation(container, frame, offsetConfirmed, commandRequestedThisFrame);
  if (step != ScrollStep::Continue) {
    return step == ScrollStep::Corrected;
  }

  return holdAnchor(container, frame, anchorKey, anchorIndex, anchorDelta, hadRowsBefore);
}

}
