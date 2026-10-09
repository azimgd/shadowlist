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
  if (operation.target.mode != AnchorMode::Element) {
    outOffset = maxOffset;
    return true;
  }
  std::size_t anchorIndex = container.findElementIndexByKey(operation.target.key);
  if (anchorIndex == UNDEFINED_INDEX) {
    return false;
  }
  outOffset = container.getElementOffset(anchorIndex) + resolveAnchorSubOffset(container, operation, anchorIndex);
  return true;
}
}

std::vector<Anchor> Virtualizer::captureFallbackAnchors(Container& container, double inputOffset) {
  std::vector<Anchor> candidates;
  const std::vector<Element>& elements = container.revision.elements;
  std::size_t anchorIndex = container.findElementIndexByKey(container.anchor.key);
  if (anchorIndex == UNDEFINED_INDEX || container.anchor.mode != AnchorMode::Element) {
    return candidates;
  }

  /*
   * Collect the anchorable rows after the anchor that start inside the viewport, each with
   * the offset that holds it in place. In a grid, keep going until every column has passed
   * the bottom of the viewport.
   */
  double viewportEnd = inputOffset + container.getWindowContainerSize();
  std::size_t columns = container.columns > 0 ? container.columns : 1;
  std::size_t tracksPast = 0;
  for (std::size_t index = anchorIndex + 1; index < elements.size() && tracksPast < columns; ++index) {
    double elementOffset = container.getElementOffset(index);
    if (elementOffset >= viewportEnd) {
      ++tracksPast;
      continue;
    }
    if (container.isAnchorable(elements[index].key)) {
      candidates.push_back(Anchor{elements[index].key, inputOffset - elementOffset, AnchorMode::Element});
    }
  }
  return candidates;
}

void Virtualizer::captureAnchor(Container& container, double inputOffset) {
  const std::string previousAnchorKey = container.anchor.key;
  container.anchor = Anchor{"", 0.0, AnchorMode::Element};

  const std::vector<Element>& previousElements = container.revision.elements;
  if (previousElements.empty()) {
    return;
  }

  auto elementOffsetOf = [&](const Element& element) {
    return container.horizontal ? element.offsetX : element.offsetY;
  };
  auto elementSizeOf = [&](const Element& element) {
    return container.horizontal ? element.width : element.height;
  };

  /*
   * In a grid the tracks move independently as rows get measured. A new anchor each frame
   * lets the row the reader was on drift. Keep the previous anchor while it is still on screen.
   */
  if (container.columns > 1 && !previousAnchorKey.empty() && container.isAnchorable(previousAnchorKey)) {
    std::size_t previousAnchorIndex = container.findElementIndexByKey(previousAnchorKey);
    if (previousAnchorIndex != UNDEFINED_INDEX) {
      const Element& previousAnchor = previousElements[previousAnchorIndex];
      double start = elementOffsetOf(previousAnchor);
      if (start + elementSizeOf(previousAnchor) > inputOffset && start < inputOffset + container.getWindowContainerSize()) {
        container.anchor = Anchor{previousAnchorKey, inputOffset - start, AnchorMode::Element};
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
  if (container.columns <= 1) {
    std::size_t low = 0;
    std::size_t high = previousElements.size();
    while (low < high) {
      std::size_t mid = low + (high - low) / 2;
      const Element& element = previousElements[mid];
      if (elementOffsetOf(element) + elementSizeOf(element) <= inputOffset) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    scanStart = low;
  }

  const Element* firstPast = nullptr;
  for (std::size_t previousElementIndex = scanStart; previousElementIndex < previousElements.size(); ++previousElementIndex) {
    const Element& previousElement = previousElements[previousElementIndex];
    double elementOffset = elementOffsetOf(previousElement);
    double elementSize = elementSizeOf(previousElement);

    if (elementOffset + elementSize <= inputOffset) {
      continue;
    }
    if (firstPast == nullptr) {
      firstPast = &previousElement;
    }
    if (container.isAnchorable(previousElement.key)) {
      /*
       * A row with only a guessed size may change size in several steps, moving everything
       * below it each time. This is common right after a prepend at the top. If a row with a
       * known size starts inside the viewport, anchor that one instead.
       * Only for single column lists that aren't inverted. Inverted lists use their anchor
       * to tell whether they rest on the newest row.
       */
      const Element* anchorElement = &previousElement;
      if (container.columns <= 1 && !container.inverted && !previousElement.measured && !previousElement.predicted) {
        double viewportEnd = inputOffset + container.getWindowContainerSize();
        for (std::size_t laterIndex = previousElementIndex + 1; laterIndex < previousElements.size(); ++laterIndex) {
          const Element& laterElement = previousElements[laterIndex];
          if (elementOffsetOf(laterElement) >= viewportEnd) {
            break;
          }
          if (container.isAnchorable(laterElement.key) && (laterElement.measured || laterElement.predicted)) {
            anchorElement = &laterElement;
            break;
          }
        }
      }
      container.anchor =
        Anchor{anchorElement->key, inputOffset - elementOffsetOf(*anchorElement), AnchorMode::Element};
      return;
    }
  }

  /*
   * No content row is in or below the viewport. Use the last content row, then the top row
   * even if it is decoration, then the very last row. There is always an anchor.
   */
  for (auto reverse = previousElements.rbegin(); reverse != previousElements.rend(); ++reverse) {
    if (container.isAnchorable(reverse->key)) {
      container.anchor = Anchor{reverse->key, inputOffset - elementOffsetOf(*reverse), AnchorMode::Element};
      return;
    }
  }
  const Element& fallback = firstPast != nullptr ? *firstPast : previousElements.back();
  container.anchor = Anchor{fallback.key, inputOffset - elementOffsetOf(fallback), AnchorMode::Element};
}

bool Virtualizer::resolveScroll(
  Container& container,
  const std::string& anchorKey,
  double anchorDelta,
  bool hadElementsBefore,
  bool offsetConfirmed) {
  std::size_t elementsSize = container.revision.elements.size();
  container.containerOffsetCorrected = false;

  // An emptied list starts over. An inverted list sticks to the bottom again when rows arrive.
  if (elementsSize == 0) {
    container.invertedInitialized = false;
    container.invertedBottomReleased = false;
    container.invertedOpeningPin = false;
    container.operation.reset();
    container.pendingScrollToEnd = false;
  }

  double windowSize = container.getWindowContainerSize();
  double totalSize = container.horizontal ? container.revision.totalContainerWidth : container.revision.totalContainerHeight;
  double currentOffset = container.horizontal ? container.revision.containerOffsetX : container.revision.containerOffsetY;
  double maxOffset = totalSize - windowSize;
  if (maxOffset < 0.0) {
    maxOffset = 0.0;
  }

  // Track the total every frame so step 2b can tell when the bottom stops growing.
  double previousTotalForScrollToEnd = container.pendingScrollToEndLastTotal;
  container.pendingScrollToEndLastTotal = totalSize;

  auto clampOffset = [&](double offset) {
    if (offset < 0.0) {
      offset = 0.0;
    }
    if (offset > maxOffset) {
      offset = maxOffset;
    }
    return offset;
  };

  /*
   * Reuse the id when the same correction is requested again. The host's reply then still
   * matches it over several frames. A new type or a new row gets a new id.
   */
  auto operationId = [&](OperationType type, AnchorMode mode, const std::string& key) -> std::uint64_t {
    if (container.operation && container.operation->type == type &&
        container.operation->target.mode == mode &&
        (mode != AnchorMode::Element || container.operation->target.key == key)) {
      return container.operation->id;
    }
    return container.nextOperationId++;
  };

  // Start or repeat a correction that aims at the end, until the view gets there.
  auto requestFixed = [&](OperationType type) {
    if (container.operation && container.operation->type != type) {
      SL_LOG("  op replaced: type=%d key=%s -> type=%d", static_cast<int>(container.operation->type),
        container.operation->target.key.c_str(), static_cast<int>(type));
    }
    container.operation =
      Operation{operationId(type, AnchorMode::EndEdge, ""), type, Anchor{"", 0.0, AnchorMode::EndEdge}};
  };

  /*
   * Start or repeat a correction that aims at a row. The target is looked up by key each
   * frame. It follows the row while nearby rows get measured.
   */
  auto requestAnchor = [&](OperationType type, const std::string& key, double delta, double viewPosition = 0.0,
                         double rowOffset = 0.0) {
    if (container.operation && (container.operation->type != type || container.operation->target.key != key)) {
      SL_LOG("  op replaced: type=%d key=%s -> type=%d key=%s", static_cast<int>(container.operation->type),
        container.operation->target.key.c_str(), static_cast<int>(type), key.c_str());
    }
    container.operation =
      Operation{operationId(type, AnchorMode::Element, key), type, Anchor{key, delta, AnchorMode::Element}, viewPosition,
        rowOffset};
  };

  /*
   * Set when a scroll command arrives this frame. The command owns the offset. Holding
   * the old visible content must not pull the view back.
   */
  bool commandRequestedThisFrame = false;

  /*
   * Step 0. Content shrank and left the view past the new end, like collapsing a tree.
   * Go back to the bottom. Checking for a shrink avoids fighting a bounce.
   * It runs as an operation so it survives recommits, and step 3 clears it on arrival.
   */
  if (!container.inverted && elementsSize > 0 &&
      currentOffset > maxOffset + OFFSET_MOVED_THRESHOLD &&
      totalSize < previousTotalForScrollToEnd) {
    /*
     * Only when no anchor will place the view. The shrink is often above the viewport, and
     * the row on screen moved up with it. Steps 3 and 4 follow that row and clamp when
     * needed. Jumping to the end would show rows further down instead.
     */
    bool anchorPlacesView = false;
    if (container.operation && container.operation->target.mode == AnchorMode::Element) {
      anchorPlacesView = container.findElementIndexByKey(container.operation->target.key) != UNDEFINED_INDEX;
    } else if (!container.operation && hadElementsBefore && !anchorKey.empty()) {
      std::size_t anchorIndex = container.findElementIndexByKey(anchorKey);
      anchorPlacesView = anchorIndex != UNDEFINED_INDEX &&
        std::fabs(container.getElementOffset(anchorIndex) + anchorDelta - currentOffset) >= OFFSET_MOVED_THRESHOLD;
    }
    if (!anchorPlacesView) {
      requestFixed(OperationType::ShrinkClamp);
    }
  }

  // Step 1. Scroll to an index.
  if (container.scrollToIndexTarget != UNDEFINED_INDEX) {
    if (container.scrollToIndexTarget < elementsSize) {
      /*
       * Follow the row itself, not a fixed offset. Its offset is a guess at first, and
       * following the row settles on it as the area gets measured.
       */
      const std::string targetKey = container.getElementAtIndex(container.scrollToIndexTarget).key;
      // The view position travels with the operation and is applied again each frame.
      requestAnchor(OperationType::ScrollToKey, targetKey, 0.0, container.scrollToIndexViewPosition,
        container.scrollToIndexRowOffset);
      commandRequestedThisFrame = true;
      // This replaces the bottom pin, but only for an index in range.
      container.invertedInitialized = true;
      container.invertedOpeningPin = false;
    }
    container.scrollToIndexTarget = UNDEFINED_INDEX;
  }

  /*
   * Step 1b. Scroll to the start. It holds the first row below the header so it keeps up
   * with measuring. Momentum neither cancels nor moves it because it is a command.
   */
  if (container.pendingScrollToStart) {
    container.pendingScrollToStart = false;
    if (elementsSize > 0) {
      std::size_t leading = 0;
      if (container.getElementOffset(elementsSize - 1) < container.getElementOffset(0)) {
        leading = elementsSize - 1;
      }
      requestAnchor(
        OperationType::ScrollToStart,
        container.getElementAtIndex(leading).key,
        -container.getElementOffset(leading));
      commandRequestedThisFrame = true;
      container.invertedInitialized = true;
      container.invertedOpeningPin = false;
    }
  }

  /*
   * Step 2. An inverted list sticks to the bottom until it gets there, following it as the
   * content grows. Wait for the window size so the target is right.
   */
  if (container.inverted && !container.invertedInitialized && elementsSize > 0 && windowSize > 0.0) {
    requestFixed(OperationType::BottomPin);
    container.invertedOpeningPin = true;
    // Done only once the list can scroll and sits at the bottom. Until then keep pinning.
    if (offsetConfirmed &&
        totalSize > windowSize &&
        std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD) {
      container.invertedInitialized = true;
    }
  }

  /*
   * Step 2b. Scroll to the end, aiming again every frame as the content grows. It is done
   * when the view is at the bottom, the total held still and the last row has a real size.
   * A drag cancels it, momentum does not.
   */
  if (container.pendingScrollToEnd && elementsSize > 0 && windowSize > 0.0) {
    container.invertedOpeningPin = false;
    bool atBottom = std::fabs(currentOffset - maxOffset) < OFFSET_ARRIVED_THRESHOLD;
    bool totalStable = totalSize == previousTotalForScrollToEnd;
    if (atBottom && totalStable && container.hasTrustedSize(elementsSize - 1)) {
      container.pendingScrollToEnd = false;
    } else {
      requestFixed(OperationType::ScrollToEnd);
    }
  }

  /*
   * Step 3. Keep driving the running operation until the view gets there. The target is
   * worked out again each frame. It ends when its row is gone or the view arrives.
   */
  if (container.operation) {
    double rawTarget = 0.0;
    if (!resolveAnchorOffset(container, *container.operation, maxOffset, rawTarget)) {
      container.operation.reset();
    } else {
      double target = clampOffset(rawTarget);
      /*
       * A host report at the target ends the operation. So does our own write, if the host
       * last reported the target too. Without that, a resting list only sees its own writes
       * and republishes on every commit, about 500 times for one streaming reply.
       */
      bool reportedAtTarget = std::fabs(container.lastReportedOffset - target) < OFFSET_ARRIVED_THRESHOLD;
      if ((offsetConfirmed || reportedAtTarget) &&
          std::fabs(currentOffset - target) < OFFSET_ARRIVED_THRESHOLD) {
        SL_LOG("  op arrived: type=%d key=%s index=%zd target=%.1f", static_cast<int>(container.operation->type),
          container.operation->target.key.c_str(),
          static_cast<std::ptrdiff_t>(container.operation->target.mode == AnchorMode::Element
            ? container.findElementIndexByKey(container.operation->target.key) : UNDEFINED_INDEX),
          target);
        container.operation.reset();
        /*
         * A command that is already where it wants to be is done. Holding the old anchor
         * would move the view to a row further down after a data swap.
         */
        if (commandRequestedThisFrame) {
          return false;
        }
      } else {
        // The offset moved. The caller picks the window again.
        correctOffset(container, target);
        return true;
      }
    }
  }

  /*
   * Step 4. With no correction running, keep the anchor row where it was on screen.
   * Rows added or removed above start a correction. Plain scrolling does nothing.
   */
  if (hadElementsBefore && !anchorKey.empty()) {
    std::size_t anchorIndex = container.findElementIndexByKey(anchorKey);
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
        ? maxOffset
        : container.getElementOffset(anchorIndex) + anchorDelta;
      if (std::fabs(rawAnchoredOffset - currentOffset) >= OFFSET_MOVED_THRESHOLD) {
        if (invertedBottomAnchor) {
          requestFixed(OperationType::BottomPin);
        } else {
          requestAnchor(OperationType::MaintainAnchor, anchorKey, anchorDelta);
        }
        correctOffset(container, clampOffset(rawAnchoredOffset));
        return true;
      }
    }
  }

  return false;
}

}
