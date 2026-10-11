#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/VirtualizerInternal.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace azimgd::shadowlist {

namespace {
/*
 * Correct the offset only when probe is a real move from the current offset.
 * Probe is the target before any clamp. It equals offset unless the caller clamps.
 */
bool correctOffsetIfMoved(Container& container, double offset, double probe) {
  if (std::fabs(probe - scrollAxisOffset(container)) < OFFSET_MOVED_THRESHOLD) {
    return false;
  }
  correctOffset(container, offset);
  return true;
}

/*
 * Move the running row correction and the captured anchor by the same delta.
 */
void shiftAnchors(Container& container, double delta) {
  if (container.operation && container.operation->target.mode == AnchorMode::Row) {
    container.operation->target.offset += delta;
  }
  if (!container.anchor.key.empty() && container.anchor.mode == AnchorMode::Row) {
    container.anchor.offset += delta;
  }
}
}

bool Virtualizer::applyRowSize(Container& container, std::size_t index, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  if (index >= container.revision.rows.size()) {
    throw InvalidOperationError("Index out of bounds");
  }

  Row& nextRow = container.revision.rows[index];

  double previousWidth = nextRow.width;
  double previousHeight = nextRow.height;
  bool wasMeasured = nextRow.measured;
  bool dimensionsChanged = previousWidth != size.width || previousHeight != size.height;

  /*
   * Already measured at this size. Fabric reports every mounted row on every layout pass.
   * Most calls stop here. Without this, each one would reflow the rest of the list.
   */
  if (!dimensionsChanged && wasMeasured) {
    return false;
  }

  // On the anchor row's first measurement, remember how far its bottom edge moved.
  if (!wasMeasured && dimensionsChanged && container.numberOfColumns <= 1) {
    const Anchor* compensationAnchor = container.getCompensationAnchor();
    if (compensationAnchor != nullptr && !compensationAnchor->key.empty() && nextRow.key == compensationAnchor->key) {
      container.anchorFirstMeasurementDelta += container.horizontal
        ? size.width - previousWidth
        : size.height - previousHeight;
    }
  }

  nextRow.width = size.width;
  nextRow.height = size.height;
  nextRow.estimated = true;
  nextRow.measured = true;

  // A real measurement always replaces a prediction.
  nextRow.predicted = false;

  // Limit the coming reflow without scheduling a second one in the layout pass.
  container.noteRowSizeSpan(index);

  // Add to the running total for the average. A remeasure only adds the difference.
  if (wasMeasured) {
    container.revision.measuredRealTotalWidth += size.width - previousWidth;
    container.revision.measuredRealTotalHeight += size.height - previousHeight;
  } else {
    container.revision.measuredRealCount++;
    container.revision.measuredRealTotalWidth += size.width;
    container.revision.measuredRealTotalHeight += size.height;
  }

  // A first measurement equal to the estimate still counts for the average but moves nothing.
  return dimensionsChanged;
}

std::size_t Virtualizer::applyPredictedRowSize(Container& container, const std::string& key, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  if (key.empty()) {
    return UNDEFINED_INDEX;
  }

  std::size_t index = container.indexOfKey(key);

  // The row doesn't exist yet, which is normal when measuring ahead. Save it for the next update.
  if (index >= container.revision.rows.size()) {
    container.setPredictedSize(key, size);
    return UNDEFINED_INDEX;
  }

  Row& nextRow = container.revision.rows[index];

  // A real measurement always wins. Drop a late prediction so it can't come back later.
  if (nextRow.measured) {
    return UNDEFINED_INDEX;
  }

  bool dimensionsChanged = nextRow.width != size.width || nextRow.height != size.height;

  nextRow.width = size.width;
  nextRow.height = size.height;
  nextRow.estimated = true;
  nextRow.predicted = true;

  // Predictions stay out of the average, which only counts real measurements.
  if (!dimensionsChanged) {
    return UNDEFINED_INDEX;
  }

  container.markRowSizeDirty(index);

  return index;
}

void Virtualizer::commitRowSizes(Container& container, std::size_t fromIndex) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  if (fromIndex >= container.revision.rows.size()) {
    return;
  }

  // Only this row and the ones after it move. The caller updates the total once per batch.
  recomputeRowOffsets(container, fromIndex, container.rowSizeDirtyToIndex);
  container.rowSizeDirtyToIndex = 0;

  /*
   * Keep the anchor row still while rows off screen get measured.
   * Corrections that aim at a fixed offset, like the bottom, handle themselves.
   */
  const Anchor* compensationAnchor = container.getCompensationAnchor();
  if (compensationAnchor != nullptr) {
    double compensationDelta = compensationAnchor->offset;
    std::size_t anchorIndex = container.indexOfKey(compensationAnchor->key);
    if (anchorIndex != UNDEFINED_INDEX) {
      // A scroll to a key works its sub offset out from the view position, like resolveScroll does.
      if (container.operation && compensationAnchor == &container.operation->target) {
        compensationDelta = resolveAnchorOffset(container, *container.operation, anchorIndex);
      }
      // Compare the target before clamping. A bounce at the top is left alone.
      double rawAnchoredOffset = container.getRowOffset(anchorIndex) + compensationDelta;
      /*
       * When the anchor row starts above the viewport, the reader sees its bottom part.
       * On its first measurement, hold its bottom edge so the size error lands off screen.
       */
      if (compensationDelta > 0.0) {
        rawAnchoredOffset += container.anchorFirstMeasurementDelta;
      }
      double anchoredOffset = rawAnchoredOffset < 0.0 ? 0.0 : rawAnchoredOffset;
      /*
       * Clamp the top end against the reflowed rows, since the stored total is stale here.
       * Rows near the end that measure shorter than their estimate pull the end in. A host
       * clamps a write past it, and a later correction that shifts a moving view from the
       * unclamped offset would apply the difference twice. A view already past the end is
       * in a bounce and is left alone.
       */
      double windowSize = container.getWindowSize();
      double staleTotal = container.horizontal ? container.revision.contentWidth
                                               : container.revision.contentHeight;
      if (scrollAxisOffset(container) <= std::max(0.0, staleTotal - windowSize) + OFFSET_MOVED_THRESHOLD) {
        Size extent = tailExtent(container);
        double contentEnd = container.horizontal ? extent.width : extent.height;
        double total = std::max(contentEnd, container.headerSize) + container.footerSize;
        anchoredOffset = std::min(anchoredOffset, std::max(0.0, total - windowSize));
      }
      correctOffsetIfMoved(container, anchoredOffset, rawAnchoredOffset);
    }
  }
  container.anchorFirstMeasurementDelta = 0.0;

  /*
   * An inverted list resting on its newest row follows that row as it grows. Otherwise the
   * growth lands below the fold, and a reply that finishes after the last commit gets no
   * next frame to fix it. It only follows when the reader hasn't scrolled away, no finger
   * is down and no correction is running. The stored total is stale here. The bottom
   * comes from the reflowed rows.
   * A pending scroll to the end, or a list still settling on the bottom it opened at,
   * follows the bottom here too for the same reason, and so does a running correction aimed
   * at the end, or the rows jitter for a frame as a message arrives.
   */
  bool endEdgeOperation = container.operation && container.operation->target.mode == AnchorMode::EndEdge;
  bool followBottom = container.pendingScrollToEnd || endEdgeOperation ||
    (container.inverted && container.invertedOpeningPin && container.restingAtInvertedBottom &&
     !container.invertedBottomReleased && !container.gestureActive && !container.operation);
  if (!followBottom && container.inverted && !container.invertedBottomReleased && !container.gestureActive &&
      !container.operation && !container.anchor.key.empty()) {
    followBottom = isLastAnchorable(container, container.indexOfKey(container.anchor.key));
  }
  if (followBottom) {
    Size extent = tailExtent(container);
    double contentEnd = container.horizontal ? extent.width : extent.height;
    double total = std::max(contentEnd, container.headerSize) + container.footerSize;
    double bottom = std::max(0.0, total - container.getWindowSize());
    correctOffsetIfMoved(container, bottom, bottom);
  }
}

void Virtualizer::applyHeaderSizeChange(Container& container, double previousHeaderSize) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  double delta = container.headerSize - previousHeaderSize;
  if (delta == 0.0 || container.revision.rows.empty()) {
    return;
  }

  const double offset = scrollAxisOffset(container);

  /*
   * A running anchor correction already knows where the rows belong. Resolve it against
   * the reflowed rows. Its last written offset may be clamped and can't be trusted here.
   */
  if (container.operation && container.operation->target.mode == AnchorMode::Row) {
    std::size_t anchorIndex = container.indexOfKey(container.operation->target.key);
    if (anchorIndex != UNDEFINED_INDEX) {
      // A scroll to a key works its sub offset out from the view position, like resolveScroll does.
      double target = container.getRowOffset(anchorIndex) +
        resolveAnchorOffset(container, *container.operation, anchorIndex);
      target = target < 0.0 ? 0.0 : target;
      SL_LOG("  headerSizeChange: %.1f->%.1f offset=%.1f->%.1f resolved op=%llu",
        previousHeaderSize, container.headerSize, offset, target,
        static_cast<unsigned long long>(container.operation->commitToken));
      correctOffsetIfMoved(container, target, target);
      return;
    }
  }

  // The header was fully scrolled off. Every row on screen moved. Move the offset with them.
  SL_LOG("  headerSizeChange: %.1f->%.1f offset=%.1f branch=%s anchorSub=%.1f",
    previousHeaderSize, container.headerSize, offset,
    (offset > 0.0 && offset >= previousHeaderSize) ? "hold" : "push", container.anchor.offset);
  if (offset > 0.0 && offset >= previousHeaderSize) {
    correctOffset(container, std::max(0.0, offset + delta));
    return;
  }

  /*
   * The header is on screen and pushes the rows down. Keep the offset and shift the anchor
   * instead. It still points at this offset.
   */
  shiftAnchors(container, -delta);
}

void Virtualizer::applyWindowSizeChange(Container& container, double previousWindowSize) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  double windowSize = container.getWindowSize();
  if (!container.inverted || container.revision.rows.empty() ||
      previousWindowSize <= 0.0 || windowSize <= 0.0 ||
      std::fabs(windowSize - previousWindowSize) < OFFSET_MOVED_THRESHOLD ||
      !container.invertedInitialized || container.invertedBottomReleased || container.gestureActive) {
    return;
  }

  // Judge against the old window, or a big shrink would look like the reader scrolled away.
  double total = container.horizontal ? container.revision.contentWidth : container.revision.contentHeight;
  double offset = scrollAxisOffset(container);
  if (!atInvertedBottom(offset, total, previousWindowSize)) {
    return;
  }

  /*
   * Jump to the new bottom now and keep following it as a scroll to the end, since rows
   * mounting into a bigger window get measured later.
   */
  container.pendingScrollToEnd = true;
  double bottom = std::max(0.0, total - windowSize);
  SL_LOG("  windowSizeChange: %.1f->%.1f offset=%.1f->%.1f", previousWindowSize, windowSize, offset, bottom);
  if (!correctOffsetIfMoved(container, bottom, bottom)) {
    return;
  }

  // Move the anchor too, or holding the content in place would pull the view back.
  shiftAnchors(container, bottom - offset);
}

void Virtualizer::updateRowAtIndex(Container& container, std::size_t index, Size size) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

#if SHADOWLIST_DEBUG_LOG
  double tracePreviousTotal = container.horizontal ? container.revision.contentWidth : container.revision.contentHeight;
  double tracePreviousSize = index < container.revision.rows.size()
    ? (container.horizontal ? container.revision.rows[index].width : container.revision.rows[index].height)
    : 0.0;
#endif
  if (applyRowSize(container, index, size)) {
    commitRowSizes(container, index);
    SL_LOG("  replaceChild size: index=%zu %.1f->%.1f total=%.1f corrected=%d anchor=%s",
      index, tracePreviousSize, container.horizontal ? size.width : size.height, tracePreviousTotal,
      container.offsetCorrected ? 1 : 0, container.anchor.key.c_str());
  }
}

/*
 * Throw away every prediction, saved or applied, and fall back to the estimate.
 * Used when predictions go stale, mostly on a width change since text rewraps.
 * Rows measured for real keep their size.
 */
void Virtualizer::invalidatePredictions(Container& container) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  SL_LOG("  invalidatePredictions: staged=%zu", container.predictedSizes.size());
  container.predictedSizes.clear();

  bool anyCleared = false;
  for (Row& nextRow : container.revision.rows) {
    if (!nextRow.predicted) {
      continue;
    }

    nextRow.predicted = false;
    // Clear estimated too, or the row would keep its stale predicted size forever.
    nextRow.estimated = false;
    anyCleared = true;
  }

  if (anyCleared) {
    /*
     * Predicted rows go back to the fallback size. Only the sizing loop in layoutRows
     * resets rows outside the window, and it skips itself while the fallback is unchanged.
     * Forget the last fallback to force it, then reflow the whole list.
     */
    container.previousFallbackWidth = -1.0;
    container.previousFallbackHeight = -1.0;
    container.markRowSizeDirty(0);
  }
}

/*
 * Apply saved predictions whose rows now exist.
 * Walk the few saved predictions, not the whole list. Run every frame, not just when keys
 * change, since predictions usually arrive on frames with no new data.
 */
void Virtualizer::consumePredictions(Container& container) {
  if (container.predictedSizes.empty()) {
    return;
  }

  for (auto entry = container.predictedSizes.begin(); entry != container.predictedSizes.end();) {
    std::size_t predictedIndex = container.revision.indexOfKey(entry->first);
    if (predictedIndex >= container.revision.rows.size()) {
      ++entry;
      continue;
    }

    Row& predictedRow = container.revision.rows[predictedIndex];

    // A row already measured for real ignores the prediction, and the entry is dropped.
    if (!predictedRow.measured &&
        (predictedRow.width != entry->second.width || predictedRow.height != entry->second.height)) {
      SL_LOG("  prediction: index=%zu %.1f->%.1f estimated=%d",
        predictedIndex, container.horizontal ? predictedRow.width : predictedRow.height,
        container.horizontal ? entry->second.width : entry->second.height, predictedRow.estimated ? 1 : 0);
      predictedRow.width = entry->second.width;
      predictedRow.height = entry->second.height;
      // The layout loop can't see this change. Mark it for a reflow.
      container.markRowSizeDirty(predictedIndex);
    }
    if (!predictedRow.measured) {
      predictedRow.estimated = true;
      predictedRow.predicted = true;
    }

    entry = container.predictedSizes.erase(entry);
  }
}

}
