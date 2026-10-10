#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/VirtualizerInternal.hpp>

#include <cmath>

namespace azimgd::shadowlist {

namespace {
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
 * matches it over several frames. A new type or a new row gets a new id.
 */
std::uint64_t operationId(Container& container, OperationType type, AnchorMode mode, const std::string& key) {
  if (container.operation && container.operation->type == type &&
      container.operation->target.mode == mode &&
      (mode != AnchorMode::Row || container.operation->target.key == key)) {
    return container.operation->commitToken;
  }
  return container.nextCommitToken++;
}

/*
 * Start or repeat a correction that aims at the end, until the view gets there.
 */
void requestFixed(Container& container, OperationType type) {
  if (container.operation && container.operation->type != type) {
    SL_LOG("  op replaced: type=%d key=%s -> type=%d", static_cast<int>(container.operation->type),
      container.operation->target.key.c_str(), static_cast<int>(type));
  }
  container.operation =
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
  if (container.operation && (container.operation->type != type || container.operation->target.key != key)) {
    SL_LOG("  op replaced: type=%d key=%s -> type=%d key=%s", static_cast<int>(container.operation->type),
      container.operation->target.key.c_str(), static_cast<int>(type), key.c_str());
  }
  container.operation =
    Operation{operationId(container, type, AnchorMode::Row, key), type, Anchor{key, delta, AnchorMode::Row},
      viewPosition, viewOffset};
}

/*
 * Step 0. Content shrank and left the view past the new end, like collapsing a tree.
 * Go back to the bottom. Checking for a shrink avoids fighting a bounce.
 * It runs as an operation so it survives recommits, and step 3 clears it on arrival.
 */
void requestShrinkClamp(
  Container& container,
  const ScrollFrame& frame,
  const std::string& anchorKey,
  double anchorDelta,
  bool hadRowsBefore) {
  if (!container.inverted && frame.rowCount > 0 &&
      frame.currentOffset > frame.maxOffset + OFFSET_MOVED_THRESHOLD &&
      frame.totalSize < frame.previousTotalForScrollToEnd) {
    /*
     * Only when no anchor will place the view. The shrink is often above the viewport, and
     * the row on screen moved up with it. Steps 3 and 4 follow that row and clamp when
     * needed. Jumping to the end would show rows further down instead.
     */
    bool anchorPlacesView = false;
    if (container.operation && container.operation->target.mode == AnchorMode::Row) {
      anchorPlacesView = container.indexOfKey(container.operation->target.key) != UNDEFINED_INDEX;
    } else if (!container.operation && hadRowsBefore && !anchorKey.empty()) {
      std::size_t anchorIndex = container.indexOfKey(anchorKey);
      anchorPlacesView = anchorIndex != UNDEFINED_INDEX &&
        std::fabs(container.getRowOffset(anchorIndex) + anchorDelta - frame.currentOffset) >= OFFSET_MOVED_THRESHOLD;
    }
    if (!anchorPlacesView) {
      requestFixed(container, OperationType::ShrinkClamp);
    }
  }
}

/*
 * Step 1. Scroll to an index. Returns true when the command starts this frame.
 */
bool requestScrollToRow(Container& container, const ScrollFrame& frame) {
  bool requested = false;
  if (container.scrollToRowTarget != UNDEFINED_INDEX) {
    if (container.scrollToRowTarget < frame.rowCount) {
      /*
       * Follow the row itself, not a fixed offset. Its offset is a guess at first, and
       * following the row settles on it as the area gets measured.
       */
      const std::string targetKey = container.getRowAtIndex(container.scrollToRowTarget).key;
      // The view position travels with the operation and is applied again each frame.
      requestAnchor(container, OperationType::ScrollToKey, targetKey, 0.0, container.scrollToRowViewPosition,
        container.scrollToRowViewOffset);
      requested = true;
      // This replaces the bottom pin, but only for an index in range.
      container.invertedInitialized = true;
      container.invertedOpeningPin = false;
    }
    container.scrollToRowTarget = UNDEFINED_INDEX;
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
  if (container.pendingScrollToStart) {
    container.pendingScrollToStart = false;
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
      container.invertedInitialized = true;
      container.invertedOpeningPin = false;
    }
  }
  return requested;
}

/*
 * Step 2. An inverted list sticks to the bottom until it gets there, following it as the
 * content grows. Wait for the window size so the target is right.
 */
void requestOpeningBottomPin(Container& container, const ScrollFrame& frame, bool offsetConfirmed) {
  if (container.inverted && !container.invertedInitialized && frame.rowCount > 0 && frame.windowSize > 0.0) {
    requestFixed(container, OperationType::BottomPin);
    container.invertedOpeningPin = true;
    // Done only once the list can scroll and sits at the bottom. Until then keep pinning.
    if (offsetConfirmed &&
        frame.totalSize > frame.windowSize &&
        std::fabs(frame.currentOffset - frame.maxOffset) < OFFSET_ARRIVED_THRESHOLD) {
      container.invertedInitialized = true;
    }
  }
}

/*
 * Step 2b. Scroll to the end, aiming again every frame as the content grows. It is done
 * when the view is at the bottom, the total held still and the last row has a real size.
 * A drag cancels it, momentum does not.
 */
void requestScrollToEnd(Container& container, const ScrollFrame& frame) {
  if (container.pendingScrollToEnd && frame.rowCount > 0 && frame.windowSize > 0.0) {
    container.invertedOpeningPin = false;
    bool atBottom = std::fabs(frame.currentOffset - frame.maxOffset) < OFFSET_ARRIVED_THRESHOLD;
    bool totalStable = frame.totalSize == frame.previousTotalForScrollToEnd;
    if (atBottom && totalStable && container.hasTrustedSize(frame.rowCount - 1)) {
      container.pendingScrollToEnd = false;
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
  if (container.operation) {
    double rawTarget = 0.0;
    if (!resolveAnchorOffset(container, *container.operation, frame.maxOffset, rawTarget)) {
      container.operation.reset();
    } else {
      double target = clampOffset(rawTarget, frame.maxOffset);
      /*
       * A host report at the target ends the operation. So does our own write, if the host
       * last reported the target too. Without that, a resting list only sees its own writes
       * and republishes on every commit, about 500 times for one streaming reply.
       */
      bool reportedAtTarget = std::fabs(container.previousReportedOffset - target) < OFFSET_ARRIVED_THRESHOLD;
      if ((offsetConfirmed || reportedAtTarget) &&
          std::fabs(frame.currentOffset - target) < OFFSET_ARRIVED_THRESHOLD) {
        SL_LOG("  op arrived: type=%d key=%s index=%zd target=%.1f", static_cast<int>(container.operation->type),
          container.operation->target.key.c_str(),
          static_cast<std::ptrdiff_t>(container.operation->target.mode == AnchorMode::Row
            ? container.indexOfKey(container.operation->target.key) : UNDEFINED_INDEX),
          target);
        /*
         * Grid tracks move apart as rows get measured, and one offset holds one track. Hold the
         * track of the row the operation landed on, not the one the capture picked.
         */
        const std::string& landedKey = container.operation->target.key;
        if (container.numberOfColumns > 1 && container.operation->target.mode == AnchorMode::Row &&
            container.isAnchorable(landedKey)) {
          std::size_t landedIndex = container.indexOfKey(landedKey);
          container.anchor =
            Anchor{landedKey, frame.currentOffset - container.getRowOffset(landedIndex), AnchorMode::Row};
        }
        container.operation.reset();
        /*
         * A command that is already where it wants to be is done. Holding the old anchor
         * would move the view to a row further down after a data swap.
         */
        if (commandRequestedThisFrame) {
          return ScrollStep::Done;
        }
      } else {
        // The offset moved. The caller picks the window again.
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
  double anchorDelta,
  bool hadRowsBefore) {
  if (hadRowsBefore && !anchorKey.empty()) {
    std::size_t anchorIndex = container.indexOfKey(anchorKey);
    if (anchorIndex != UNDEFINED_INDEX) {
      /*
       * An inverted list resting on its newest content row holds the true bottom instead of
       * the row's old position. The row can grow after first paint, and holding its position
       * would leave a gap at the bottom and fight the bottom pin. Trailing decoration rows
       * don't count as the newest row.
       * Not when the user has scrolled up, or the view gets pulled from under them. Not while
       * a finger is down either, or the content snaps back on every touch frame.
       * A list still settling on the bottom it opened at always holds the bottom.
       */
      bool invertedBottomAnchor = false;
      if (container.inverted && !container.invertedBottomReleased && !container.gestureActive) {
        if (container.invertedOpeningPin && container.restingAtInvertedBottom) {
          invertedBottomAnchor = true;
        } else {
          invertedBottomAnchor = isLastAnchorable(container, anchorIndex);
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

std::vector<Anchor> Virtualizer::captureFallbackAnchors(Container& container, double inputOffset) {
  std::vector<Anchor> candidates;
  const std::vector<Row>& rows = container.revision.rows;
  std::size_t anchorIndex = container.indexOfKey(container.anchor.key);
  if (anchorIndex == UNDEFINED_INDEX || container.anchor.mode != AnchorMode::Row) {
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

void Virtualizer::captureAnchor(Container& container, double inputOffset) {
  const std::string previousAnchorKey = container.anchor.key;
  container.anchor = Anchor{"", 0.0, AnchorMode::Row};

  const std::vector<Row>& previousRows = container.revision.rows;
  if (previousRows.empty()) {
    return;
  }

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
  if (container.numberOfColumns > 1 && !previousAnchorKey.empty() && container.isAnchorable(previousAnchorKey)) {
    std::size_t previousAnchorIndex = container.indexOfKey(previousAnchorKey);
    if (previousAnchorIndex != UNDEFINED_INDEX) {
      const Row& previousAnchor = previousRows[previousAnchorIndex];
      double start = rowOffsetOf(previousAnchor);
      if (start + rowSizeOf(previousAnchor) > inputOffset && start < inputOffset + container.getWindowSize()) {
        container.anchor = Anchor{previousAnchorKey, inputOffset - start, AnchorMode::Row};
        return;
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

  const Row* firstPast = nullptr;
  for (std::size_t previousRowIndex = scanStart; previousRowIndex < previousRows.size(); ++previousRowIndex) {
    const Row& previousRow = previousRows[previousRowIndex];
    double rowOffset = rowOffsetOf(previousRow);
    double rowSize = rowSizeOf(previousRow);

    if (rowOffset + rowSize <= inputOffset) {
      continue;
    }
    if (firstPast == nullptr) {
      firstPast = &previousRow;
    }
    if (container.isAnchorable(previousRow.key)) {
      /*
       * A row with only a guessed size may change size in several steps, moving everything
       * below it each time. This is common right after a prepend at the top. If a row with a
       * known size starts inside the viewport, anchor that one instead.
       * Only for single column lists that aren't inverted. Inverted lists use their anchor
       * to tell whether they rest on the newest row.
       */
      const Row* anchorRow = &previousRow;
      if (container.numberOfColumns <= 1 && !container.inverted && !previousRow.measured && !previousRow.predicted) {
        double viewportEnd = inputOffset + container.getWindowSize();
        for (std::size_t laterIndex = previousRowIndex + 1; laterIndex < previousRows.size(); ++laterIndex) {
          const Row& laterRow = previousRows[laterIndex];
          if (rowOffsetOf(laterRow) >= viewportEnd) {
            break;
          }
          if (container.isAnchorable(laterRow.key) && (laterRow.measured || laterRow.predicted)) {
            anchorRow = &laterRow;
            break;
          }
        }
      }
      container.anchor =
        Anchor{anchorRow->key, inputOffset - rowOffsetOf(*anchorRow), AnchorMode::Row};
      return;
    }
  }

  /*
   * No content row is in or below the viewport. Use the last content row, then the top row
   * even if it is decoration, then the very last row. There is always an anchor.
   */
  for (auto reverse = previousRows.rbegin(); reverse != previousRows.rend(); ++reverse) {
    if (container.isAnchorable(reverse->key)) {
      container.anchor = Anchor{reverse->key, inputOffset - rowOffsetOf(*reverse), AnchorMode::Row};
      return;
    }
  }
  const Row& fallback = firstPast != nullptr ? *firstPast : previousRows.back();
  container.anchor = Anchor{fallback.key, inputOffset - rowOffsetOf(fallback), AnchorMode::Row};
}

bool Virtualizer::resolveScroll(
  Container& container,
  const std::string& anchorKey,
  double anchorDelta,
  bool hadRowsBefore,
  bool offsetConfirmed) {
  std::size_t rowCount = container.revision.rows.size();
  container.offsetCorrected = false;

  // An emptied list starts over. An inverted list sticks to the bottom again when rows arrive.
  if (rowCount == 0) {
    container.invertedInitialized = false;
    container.invertedBottomReleased = false;
    container.invertedOpeningPin = false;
    container.operation.reset();
    container.pendingScrollToEnd = false;
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
  frame.previousTotalForScrollToEnd = container.pendingScrollToEndPreviousTotal;
  container.pendingScrollToEndPreviousTotal = frame.totalSize;

  requestShrinkClamp(container, frame, anchorKey, anchorDelta, hadRowsBefore);

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

  return holdAnchor(container, frame, anchorKey, anchorDelta, hadRowsBefore);
}

}
