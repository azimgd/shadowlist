#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/VirtualizerInternal.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace azimgd::shadowlist {

/*
 * Debug only. Gives the key at an index so native and JS logs can be matched by content,
 * since an index points at different rows during a prepend. It is only used inside SL_LOG.
 * It is marked maybe_unused to keep Android builds with -Werror=unused-function clean.
 */
namespace {
[[maybe_unused]] const char* debugKeyAt(const Container& container, std::size_t index) {
  if (index < container.revision.rows.size()) {
    const std::string& key = container.revision.rows[index].key;
    return key.empty() ? "(empty)" : key.c_str();
  }
  return "(oob)";
}

/*
 * Give a row the fallback size if it has none and widen the measured range to include it.
 * Returns false when there is no estimate to give it.
 */
bool visitMeasuredRow(
  Container& container,
  std::size_t nextRowIndex,
  std::size_t& measuredLow,
  std::size_t& measuredHigh) {
  Row& nextRow = container.revision.rows[nextRowIndex];

  if (!nextRow.estimated) {
    auto [width, height] = effectiveFallbackSize(container);

    if (width == 0.0 && height == 0.0) {
      return false;
    }

    /*
     * Layout already gave unmeasured rows this size. Usually nothing changes here.
     * Only mark sizes dirty on a real change. Otherwise dragging the scroll indicator
     * would reflow the whole list on every frame.
     */
    if (nextRow.width != width || nextRow.height != height) {
      nextRow.width = width;
      nextRow.height = height;
      // The size changed outside the layout loop. Make sure it reflows offsets.
      container.markRowSizeDirty(nextRowIndex);
    }
    nextRow.estimated = true;
  }

  if (measuredLow == UNDEFINED_INDEX || nextRowIndex < measuredLow) {
    measuredLow = nextRowIndex;
  }
  if (measuredHigh == UNDEFINED_INDEX || nextRowIndex > measuredHigh) {
    measuredHigh = nextRowIndex;
  }
  return true;
}
}

void Virtualizer::update(Container& container, const FrameInput& input) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  // The keys may be borrowed from the caller. They are only valid during this call.
  const std::vector<std::string>& inputKeys = input.getKeyList();

  SL_LOG("update: keys=%zu prevRows=%zu off=(%.1f,%.1f) win=(%.1f,%.1f) inv=%d cols=%zu hdr=%.1f ftr=%.1f invInit=%d total=%.1f dirtyFrom=%zd enabled=%d corrected=%d coreOff=%.1f",
    inputKeys.size(), container.revision.rows.size(),
    input.offsetX, input.offsetY,
    input.windowWidth, input.windowHeight,
    input.inverted ? 1 : 0, input.numberOfColumns, input.headerSize, input.footerSize,
    container.invertedInitialized ? 1 : 0,
    container.horizontal ? container.revision.contentWidth : container.revision.contentHeight,
    static_cast<std::ptrdiff_t>(container.rowSizeDirtyFromIndex), input.offsetEnabled ? 1 : 0,
    container.offsetCorrected ? 1 : 0, container.getOffset());

  // Keep the previous header size to settle a change after the rows reflow.
  double previousHeaderSize = container.headerSize;
  applyFrameInput(container, input);

  double inputOffset = container.horizontal ? input.offsetX : input.offsetY;

  /*
   * An enabled offset is our own write coming back before the host applied it, not a host
   * report. It can't confirm the correction or count as gesture travel.
   */
  bool coreOffsetWrite = input.offsetEnabled;
  bool gestureTakeover = applyGestureState(container, input, inputOffset, coreOffsetWrite);
  bool restingAtBottom = applyInvertedBottomPin(container, input, inputOffset, gestureTakeover);
  // Our own write is not where the host is. Don't record it.
  if (!coreOffsetWrite) {
    container.previousReportedOffset = inputOffset;
  }

  // Capture the anchor row so the same content stays in view through the reconcile.
  bool hadRowsBefore = !container.revision.rows.empty();
  captureAnchor(container, inputOffset);

  std::string anchorKey = container.anchor.key;
  double anchorDelta = container.anchor.offset;
  reconcileFrameKeys(container, input, inputKeys, inputOffset, restingAtBottom, hadRowsBefore, anchorKey, anchorDelta);

  measureFrame(container, input, previousHeaderSize, hadRowsBefore, anchorKey, anchorDelta);

  // The commit token is just the running operation's id. No operation means no token.
  SL_LOG("  resolved: offset=(%.1f,%.1f) corrected=%d invInit=%d token=%llu",
    container.revision.offsetX, container.revision.offsetY,
    container.offsetCorrected ? 1 : 0, container.invertedInitialized ? 1 : 0,
    static_cast<unsigned long long>(container.operation ? container.operation->commitToken : 0));

  if (container.gestureActive && container.operation) {
    container.gestureCommitToken = container.operation->commitToken;
  }

  container.endRevision();
}

void Virtualizer::applyFrameInput(Container& container, const FrameInput& input) {
  const std::vector<std::string>& inputNonAnchorKeys = input.getNonAnchorKeyList();

  // Flipping the list order moves the bottom. Start following the bottom again.
  if (container.inverted != input.inverted) {
    container.invertedBottomReleased = false;
  }

  container.inverted = input.inverted;
  container.horizontal = input.horizontal;
  container.numberOfColumns = input.numberOfColumns;
  container.overscan = input.overscan;
  container.headerSize = input.headerSize;
  container.footerSize = input.footerSize;
  // Compare first. An unchanged list costs no copy.
  const std::vector<std::size_t>& inputStickyIndices = input.getStickyIndexList();
  if (container.stickyIndices != inputStickyIndices) {
    container.stickyIndices = inputStickyIndices;
  }
  container.startReachedThreshold = input.startReachedThreshold;
  container.endReachedThreshold = input.endReachedThreshold;
  // Compare first. Rules rarely change and a copy costs an allocation.
  const std::vector<ViewableRule>& inputViewableRules = input.getViewableRules();
  if (container.viewableRules != inputViewableRules) {
    container.viewableRules = inputViewableRules;
  }
  container.estimatedRowSize = input.estimatedRowSize;
  container.snapToItem = input.snapToItem;
  container.snapAlignment = input.snapAlignment;

  /*
   * Decoration rows must never become the anchor. This runs before captureAnchor reads it,
   * and is rebuilt every frame so a row switching roles takes effect right away.
   */
  if (!input.nonAnchorKeysUnchanged &&
      (container.nonAnchorKeys.size() != inputNonAnchorKeys.size() ||
       !std::all_of(inputNonAnchorKeys.begin(), inputNonAnchorKeys.end(),
         [&](const std::string& ignoredKey) { return container.nonAnchorKeys.count(ignoredKey) != 0; }))) {
    container.nonAnchorKeys.clear();
    for (const std::string& ignoredKey : inputNonAnchorKeys) {
      container.nonAnchorKeys.insert(ignoredKey);
    }
  }
}

bool Virtualizer::applyGestureState(
  Container& container,
  const FrameInput& input,
  double inputOffset,
  bool coreOffsetWrite) {
  // A running correction survives while the offset has not moved. The user is not scrolling.
  bool userMovedOffset = !coreOffsetWrite &&
    std::fabs(inputOffset - container.previousReportedOffset) >= OFFSET_MOVED_THRESHOLD;
  /*
   * When the user takes over, drop any running correction and stop pinning to the bottom.
   * The host's gesture phase decides. userScrolled only counts with a real move. A stale
   * flag can't cancel a correction and hosts without a phase still work.
   */
  bool gestureTakeover =
    (input.userScrolled && userMovedOffset) ||
    input.scrollPhase == ScrollPhase::Dragging ||
    input.scrollPhase == ScrollPhase::Settling;
  /*
   * Only a finger, or a user move that isn't momentum, cancels a scroll command.
   * A command sent during a fling must still run, even though its first frames report
   * the settling phase. The host stops the fling itself.
   */
  bool dragTakeover =
    input.scrollPhase == ScrollPhase::Dragging ||
    (input.userScrolled && userMovedOffset && input.scrollPhase != ScrollPhase::Settling);
  bool scrollCommandInFlight = container.operation &&
    (container.operation->type == OperationType::ScrollToEnd ||
     container.operation->type == OperationType::ScrollToKey ||
     container.operation->type == OperationType::ScrollToStart);
  /*
   * A correction that keeps the visible content in place is not something a gesture cancels.
   * A prepend during a fling runs update twice on the same settling report, and dropping the
   * correction on the second pass would leave the view unmoved. So keep it until the host
   * reports back its commit token, and shift it by any momentum travel before then.
   */
  bool maintainingAnchor = container.operation &&
    container.operation->type == OperationType::MaintainAnchor &&
    container.operation->target.mode == AnchorMode::Row;
  bool echoesOperation = container.operation && !coreOffsetWrite &&
    input.commitToken == container.operation->commitToken;
  /*
   * Scrolling reported before the host applies the correction moves its target along.
   * A correction started during a gesture keeps following after the frames go idle, since
   * the end of a bounce still moves the offset. Other idle moves, like a host clamping to
   * shorter content, are not followed.
   */
  if (maintainingAnchor && !echoesOperation && userMovedOffset &&
      (gestureTakeover || container.operation->commitToken == container.gestureCommitToken)) {
    container.operation->target.offset += inputOffset - container.previousReportedOffset;
  }
  // A correction made during a gesture is done once the host reports it back while idle.
  if (maintainingAnchor && echoesOperation && !gestureTakeover &&
      container.operation->commitToken == container.gestureCommitToken) {
    container.operation.reset();
  }
  if (gestureTakeover) {
    bool keepsAnchorCorrection = maintainingAnchor && !echoesOperation;
    if (!keepsAnchorCorrection && (!scrollCommandInFlight || dragTakeover)) {
      container.operation.reset();
    }
    if (dragTakeover) {
      container.pendingScrollToEnd = false;
    }
    container.invertedInitialized = true;
    container.invertedOpeningPin = false;
  }
  container.gestureActive = gestureTakeover;
  return gestureTakeover;
}

bool Virtualizer::applyInvertedBottomPin(
  Container& container,
  const FrameInput& input,
  double inputOffset,
  bool gestureTakeover) {
  // Whether an inverted list rests at its bottom. Rows appended there get followed.
  bool restingAtBottom = false;

  /*
   * Pin or release the inverted list's bottom.
   * Judge it with last frame's total and window together. Mixing frames would make a
   * keyboard opening or a rotation release a reader who never moved. That is also why
   * this runs before the reconcile below.
   * Only a gesture releases the pin. Pinning again needs a real move toward the bottom made
   * by the user or a scroll command. Shrinking content or our own correction can't
   * quietly pin the reader again.
   */
  if (container.inverted) {
    double previousOffset = container.previousReportedOffset;
    double bottomTotal = container.horizontal ? container.revision.contentWidth
                                               : container.revision.contentHeight;
    double bottomWindow = container.horizontal ? container.revision.windowWidth
                                                : container.revision.windowHeight;
    bool atBottom = atInvertedBottom(inputOffset, bottomTotal, bottomWindow);
    // Content that fits the window has no bottom to leave. A bounce must not release it.
    bool scrollable = bottomTotal > bottomWindow;

    if (gestureTakeover && scrollable && !atBottom) {
      container.invertedBottomReleased = true;
    } else if (container.invertedBottomReleased && atBottom &&
        inputOffset >= previousOffset + OFFSET_MOVED_THRESHOLD &&
        (input.userScrolled || container.pendingScrollToEnd ||
         (container.operation && (container.operation->type == OperationType::ScrollToEnd ||
                                   container.operation->type == OperationType::ScrollToKey)))) {
      container.invertedBottomReleased = false;
    }

    restingAtBottom = !gestureTakeover && !container.invertedBottomReleased &&
      container.invertedInitialized && !container.revision.rows.empty() &&
      bottomWindow > 0.0 && atBottom;
  }
  // resolveScroll reads this to hold the bottom while the list opens.
  container.restingAtInvertedBottom = restingAtBottom;
  return restingAtBottom;
}

void Virtualizer::reconcileFrameKeys(
  Container& container,
  const FrameInput& input,
  const std::vector<std::string>& inputKeys,
  double inputOffset,
  bool restingAtBottom,
  bool hadRowsBefore,
  std::string& anchorKey,
  double& anchorDelta) {
  /*
   * Debug only. Log the frame where the keys changed, which is when JS and native indices
   * drift apart. oldFront@newIdx is how many rows were prepended above the old top row.
   */
#if SHADOWLIST_DEBUG_LOG
  {
    std::size_t previousSize = container.revision.rows.size();
    std::size_t nextSize = inputKeys.size();
    bool frontChanged = previousSize && nextSize && container.revision.rows.front().key != inputKeys.front();
    if (previousSize != nextSize || frontChanged) {
      std::size_t previousFrontNextIndex = UNDEFINED_INDEX;
      if (previousSize) {
        const std::string& previousFront = container.revision.rows.front().key;
        for (std::size_t nextRowIndex = 0; nextRowIndex < nextSize; ++nextRowIndex) {
          if (inputKeys[nextRowIndex] == previousFront) {
            previousFrontNextIndex = nextRowIndex;
            break;
          }
        }
      }
      SL_LOG("  RECONCILE: size %zu->%zu front '%s'->'%s' oldFront@newIdx=%zd anchorKey=%s anchorDelta=%.1f",
        previousSize, nextSize,
        previousSize ? container.revision.rows.front().key.c_str() : "(none)",
        nextSize ? inputKeys.front().c_str() : "(none)",
        static_cast<std::ptrdiff_t>(previousFrontNextIndex), anchorKey.empty() ? "(none)" : anchorKey.c_str(), anchorDelta);
    }
  }
#endif

  /*
   * Match the rows to the new keys. Most commits don't change the keys. Compare them
   * first and skip the rebuild, which is the main cost of each commit after a prepend.
   */
  bool keysChanged = !input.keysUnchanged && container.revision.rows.size() != inputKeys.size();
  if (!keysChanged && !input.keysUnchanged) {
    for (std::size_t nextRowIndex = 0; nextRowIndex < inputKeys.size(); ++nextRowIndex) {
      if (container.revision.rows[nextRowIndex].key != inputKeys[nextRowIndex]) {
        keysChanged = true;
        break;
      }
    }
  }
  if (keysChanged) {
    // New keys end the opening settle.
    container.invertedOpeningPin = false;
    /*
     * With followAppends, an inverted list resting at the bottom scrolls to new rows
     * appended below. By default they land below the fold. Following runs as a scroll to
     * the end, which yields to a drag. A reply growing in place is not an append.
     */
    std::string previousLastKey = restingAtBottom && input.followAppends
      ? container.revision.rows.back().key
      : std::string();
    std::vector<Anchor> fallbackAnchors = captureFallbackAnchors(container, inputOffset);
    std::size_t survivors = reconcileRows(container, inputKeys, input.keyEdit);
    /*
     * An inverted list whose rows were all replaced, like another conversation, opens on the
     * new bottom the way a fresh list does.
     */
    if (container.inverted && hadRowsBefore && survivors == 0 && !container.revision.rows.empty()) {
      SL_LOG("  inverted swap: %zu rows, pinning to the bottom again", container.revision.rows.size());
      container.invertedInitialized = false;
      container.invertedBottomReleased = false;
      container.invertedOpeningPin = false;
      container.operation.reset();
      container.pendingScrollToEnd = false;
      anchorKey.clear();
      fallbackAnchors.clear();
    }
    /*
     * The anchor row may have been removed, like a refresh that drops the top post while
     * adding new ones. Hold the next row that was on screen instead.
     */
    if (!anchorKey.empty() && container.indexOfKey(anchorKey) == UNDEFINED_INDEX) {
      for (const Anchor& candidate : fallbackAnchors) {
        if (container.indexOfKey(candidate.key) != UNDEFINED_INDEX) {
          SL_LOG("  anchor fallback: %s -> %s sub=%.1f", anchorKey.c_str(), candidate.key.c_str(), candidate.offset);
          container.anchor = candidate;
          anchorKey = candidate.key;
          anchorDelta = candidate.offset;
          break;
        }
      }
    }
    if (!previousLastKey.empty()) {
      std::size_t previousLastIndex = container.indexOfKey(previousLastKey);
      if (previousLastIndex != UNDEFINED_INDEX && previousLastIndex + 1 < container.revision.rows.size()) {
        container.pendingScrollToEnd = true;
      }
    }
  }
}

void Virtualizer::measureFrame(
  Container& container,
  const FrameInput& input,
  double previousHeaderSize,
  bool hadRowsBefore,
  const std::string& anchorKey,
  double anchorDelta) {
  /*
   * Apply sizes the host predicted since the last frame. This runs after the reconcile to
   * give new rows theirs, and before measure for the window to use the predicted sizes.
   */
  consumePredictions(container);

  container.revision.offsetX = input.offsetX;
  container.revision.offsetY = input.offsetY;
  double previousWindowSize = container.getWindowSize();
  container.revision.windowWidth = input.windowWidth;
  container.revision.windowHeight = input.windowHeight;
  /*
   * A window change that moves the offset, like an inverted list following its bottom as the
   * composer grows, is our own write and gets published even when nothing else corrects.
   */
  double offsetBeforeWindow = container.getOffset();
  container.offsetCorrected = false;
  applyWindowSizeChange(container, previousWindowSize);
  bool windowMovedOffset = container.offsetCorrected && container.getOffset() != offsetBeforeWindow;
  anchorDelta = container.anchor.offset;
  measure(container);

  /*
   * Settle a header size change the same way the Fabric layout pass does. The code below
   * then doesn't read it as a scroll. An offset moved here is our own write. It can't confirm
   * a running correction, and it is published even if nothing else corrects.
   */
  bool headerMovedOffset = false;
  if (hadRowsBefore && container.headerSize != previousHeaderSize) {
    double offsetBeforeHeader = container.getOffset();
    container.offsetCorrected = false;
    applyHeaderSizeChange(container, previousHeaderSize);
    anchorDelta = container.anchor.offset;
    headerMovedOffset = container.offsetCorrected && container.getOffset() != offsetBeforeHeader;
    if (headerMovedOffset) {
      measure(container, true);
    }
  }

  SL_LOG("  measured: total=(%.1f,%.1f) offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] anchorKey=%s anchor@newIdx=%zd",
    container.revision.contentWidth, container.revision.contentHeight,
    container.revision.offsetX, container.revision.offsetY,
    static_cast<std::ptrdiff_t>(container.getMeasuredRange().low),
    static_cast<std::ptrdiff_t>(container.getMeasuredRange().high),
    debugKeyAt(container, container.getMeasuredRange().low),
    debugKeyAt(container, container.getMeasuredRange().high),
    anchorKey.empty() ? "(none)" : anchorKey.c_str(),
    static_cast<std::ptrdiff_t>(anchorKey.empty() ? UNDEFINED_INDEX : container.indexOfKey(anchorKey)));

  // Apply scroll corrections, and pick the window again if the offset moved.
  bool offsetConfirmed = !input.offsetEnabled && !headerMovedOffset && !windowMovedOffset;
  bool scrollCorrected = resolveScroll(container, anchorKey, anchorDelta, hadRowsBefore, offsetConfirmed);
  if (headerMovedOffset || windowMovedOffset) {
    container.offsetCorrected = true;
  }
  if (scrollCorrected) {
    measure(container, true);
    SL_LOG("  remeasured: offset=(%.1f,%.1f) visible=[%zd..%zd] visKeys=[%s..%s] invInit=%d",
      container.revision.offsetX, container.revision.offsetY,
      static_cast<std::ptrdiff_t>(container.getMeasuredRange().low),
      static_cast<std::ptrdiff_t>(container.getMeasuredRange().high),
      debugKeyAt(container, container.getMeasuredRange().low),
      debugKeyAt(container, container.getMeasuredRange().high),
      container.invertedInitialized ? 1 : 0);
  }
}

void Virtualizer::measure(Container& container, bool rangeFromOffset) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  // Reset the measured range so it reflects only this pass.
  container.revision.measuredLow = UNDEFINED_INDEX;
  container.revision.measuredHigh = UNDEFINED_INDEX;

  /*
   * After an insert, remove or reorder, rows still hold their old offsets. Reflow first so
   * the window is chosen from real positions and no row that moved into view is left blank.
   * It also lets the window pass use its fast search instead of scanning every row.
   */
  if (container.rowStructureDirty) {
    layoutRows(container);
  }

  // The first revision fills from the edge. After a correction, use the corrected offset.
  if (!rangeFromOffset && container.revisionCount == REVISION_COUNT_FIRST) {
    measureFirstRevision(container);
  } else {
    measureNextRevision(container);
  }

  layoutRows(container);
  recomputeContentSize(container);
}

void Virtualizer::measureFirstRevision(Container& container) {
  std::size_t rowCount = container.revision.rows.size();
  double windowSize = container.getWindowSize();
  double effectiveColumns = container.numberOfColumns > 0 ? static_cast<double>(container.numberOfColumns) : 1.0;

  std::size_t measuredLow = UNDEFINED_INDEX;
  std::size_t measuredHigh = UNDEFINED_INDEX;
  double accumulated = 0.0;

  // Fill from the start, or from the end for an inverted list.
  for (std::size_t iteration = 0; iteration < rowCount; ++iteration) {
    std::size_t nextRowIndex = container.inverted ? (rowCount - 1 - iteration) : iteration;
    if (!visitMeasuredRow(container, nextRowIndex, measuredLow, measuredHigh)) {
      continue;
    }

    const Row& nextRow = container.revision.rows[nextRowIndex];
    accumulated += container.horizontal ? nextRow.width : nextRow.height;

    // Stop once the window plus the overscan buffer is full, shared across columns.
    if (accumulated / effectiveColumns >= windowSize * (1.0 + container.overscan)) {
      break;
    }
  }

  finalizeMeasurement(container, measuredLow, measuredHigh);
}

void Virtualizer::measureNextRevision(Container& container) {
  std::size_t rowCount = container.revision.rows.size();
  double offset = container.getOffset();
  double windowSize = container.getWindowSize();

  /*
   * Measure the window plus a buffer on each side so scrolling shows rows, not blanks.
   * Overscan counts in window heights. 1 means one window above and one below.
   */
  double overscanSize = windowSize * container.overscan;
  double lowerBound = offset - overscanSize;
  double upperBound = offset + windowSize + overscanSize;

  std::size_t measuredLow = UNDEFINED_INDEX;
  std::size_t measuredHigh = UNDEFINED_INDEX;

  // Before a reflow, offsets may be stale and out of order. Scan every row instead.
  bool geometryOrdered = !container.rowStructureDirty;

  auto rowOffsetAt = [&](std::size_t index) {
    const Row& row = container.revision.rows[index];
    return container.horizontal ? row.offsetX : row.offsetY;
  };
  auto rowSizeAt = [&](std::size_t index) {
    const Row& row = container.revision.rows[index];
    return container.horizontal ? row.width : row.height;
  };

  auto visit = [&](std::size_t nextRowIndex) {
    visitMeasuredRow(container, nextRowIndex, measuredLow, measuredHigh);
  };

  /*
   * Binary search a column for the first row whose end is past lowerBound.
   * This works because rows in a column sit end to end. Deep scrolls avoid walking every row.
   */
  auto seekTrack = [&](std::size_t first, std::size_t step) {
    std::size_t low = 0;
    std::size_t high = first < rowCount ? (rowCount - 1 - first) / step + 1 : 0;
    while (low < high) {
      std::size_t mid = low + (high - low) / 2;
      std::size_t index = first + mid * step;
      if (rowOffsetAt(index) + rowSizeAt(index) <= lowerBound) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    return low;
  };

  if (geometryOrdered && container.numberOfColumns <= 1) {
    for (std::size_t nextRowIndex = seekTrack(0, 1); nextRowIndex < rowCount; ++nextRowIndex) {
      if (rowOffsetAt(nextRowIndex) > upperBound) {
        break;
      }
      visit(nextRowIndex);
    }
  } else if (geometryOrdered && container.numberOfColumns > 1) {
    // Rows go to columns in turn. Each column is in order. Search each one separately.
    for (std::size_t track = 0; track < container.numberOfColumns && track < rowCount; ++track) {
      std::size_t stepsPast = seekTrack(track, container.numberOfColumns);
      for (std::size_t nextRowIndex = track + stepsPast * container.numberOfColumns;
           nextRowIndex < rowCount;
           nextRowIndex += container.numberOfColumns) {
        if (rowOffsetAt(nextRowIndex) > upperBound) {
          break;
        }
        visit(nextRowIndex);
      }
    }
  } else {
    // Offsets are not in order yet. Check every row against the window.
    for (std::size_t nextRowIndex = 0; nextRowIndex < rowCount; ++nextRowIndex) {
      double rowOffset = rowOffsetAt(nextRowIndex);
      double rowSize = rowSizeAt(nextRowIndex);

      // A row that starts above the window but still overlaps it counts too.
      if (rowOffset > upperBound || rowOffset + rowSize <= lowerBound) {
        continue;
      }
      visit(nextRowIndex);
    }
  }

  finalizeMeasurement(container, measuredLow, measuredHigh);
}

void Virtualizer::finalizeMeasurement(
  Container& container,
  std::size_t measuredLow,
  std::size_t measuredHigh) {
  container.revision.measuredLow = measuredLow;
  container.revision.measuredHigh = measuredHigh;
}

}
