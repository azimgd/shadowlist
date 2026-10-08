#include <shadowlist-core/host/ListLayout.hpp>

#include <shadowlist-core/Virtualizer.hpp>

#include <type_traits>

namespace azimgd::shadowlist {

bool applyLayoutInputs(Container& core, double headerSize, double footerSize, double windowWidth, double windowHeight) {
  bool changed = core.headerSize != headerSize || core.footerSize != footerSize ||
    core.revision.windowContainerWidth != windowWidth || core.revision.windowContainerHeight != windowHeight;
  if (!changed) {
    return false;
  }
  double previousHeaderSize = core.headerSize;
  double previousWindowSize = core.getWindowContainerSize();
  // Row offsets only depend on the header and the window's cross size.
  bool rowsMove = previousHeaderSize != headerSize ||
    (core.horizontal ? core.revision.windowContainerHeight != windowHeight
                     : core.revision.windowContainerWidth != windowWidth);
  core.headerSize = headerSize;
  core.footerSize = footerSize;
  core.revision.windowContainerWidth = windowWidth;
  core.revision.windowContainerHeight = windowHeight;
  if (rowsMove) {
    Virtualizer::recomputeElementOffsets(core, 0);
  }
  // The core's frame ran with the previous header size. Settle the change now.
  Virtualizer::applyHeaderSizeChange(core, previousHeaderSize);
  // A chat resting at its bottom keeps it as the composer resizes the list.
  Virtualizer::applyWindowSizeChange(core, previousWindowSize);
  // While the user is scrolled this just writes the current offset again.
  core.containerOffsetCorrected = true;
  return true;
}

bool SizeBatch::apply(Container& core, std::size_t elementIndex, Size size) {
  bool changed = Virtualizer::applyElementSize(core, elementIndex, size);
  if (changed && elementIndex < lowestChangedIndex_) {
    lowestChangedIndex_ = elementIndex;
  }
  return changed;
}

bool SizeBatch::commit(Container& core) {
  bool changed = lowestChangedIndex_ != UNDEFINED_INDEX;
  if (changed) {
    Virtualizer::commitElementSizes(core, lowestChangedIndex_);
  }
  Virtualizer::recomputeTotalSize(core);
  lowestChangedIndex_ = UNDEFINED_INDEX;
  return changed;
}

void applyMeasuredRows(
  Container& core,
  const std::vector<MeasuredRow>& rows,
  bool horizontal,
  std::vector<std::uint64_t>& firstMeasured) {
  SizeBatch batch;
  for (const MeasuredRow& row : rows) {
    const Element& element = core.getElementAtIndex(row.elementIndex);
    Size size{row.width, row.height};
    if (core.columns > 1) {
      if (horizontal) {
        size.height = element.height;
      } else {
        size.width = element.width;
      }
    }
    if (!element.measured) {
      firstMeasured.push_back(row.id);
    }
    batch.apply(core, row.elementIndex, size);
  }
  batch.commit(core);
}

RowFrame rowFrame(const Container& core, std::size_t elementIndex, bool horizontal) {
  const Element& element = core.getElementAtIndex(elementIndex);
  RowFrame frame;
  if (core.columns > 1) {
    frame.x = element.offsetX;
    frame.y = element.offsetY;
    frame.width = element.width;
    frame.setsWidth = true;
  } else if (horizontal) {
    frame.x = element.offsetX;
  } else {
    frame.y = element.offsetY;
  }
  return frame;
}

TemplateOffsets templateOffsets(const Container& core, double headerSize, double footerSize) {
  TemplateOffsets offsets;
  offsets.empty = headerSize;
  offsets.footer = core.getFooterOffset(footerSize);
  return offsets;
}

bool PublishedGeometry::refresh(const Container& core) {
  double windowSize = core.getWindowContainerSize();
  double totalSize = core.horizontal ? core.revision.totalContainerWidth : core.revision.totalContainerHeight;
  bool stale = geometryVersion_ != core.geometryVersion || snapToItem_ != core.snapToItem ||
    snapAlignment_ != core.snapAlignment || inverted_ != core.inverted || horizontal_ != core.horizontal ||
    windowSize_ != windowSize || totalSize_ != totalSize || sourceStickyIndices_ != core.stickyIndices;
  if (!stale) {
    return false;
  }
  geometryVersion_ = core.geometryVersion;
  snapToItem_ = core.snapToItem;
  snapAlignment_ = core.snapAlignment;
  inverted_ = core.inverted;
  horizontal_ = core.horizontal;
  windowSize_ = windowSize;
  totalSize_ = totalSize;
  sourceStickyIndices_ = core.stickyIndices;

  // Keep the previous pointer when the values did not change.
  auto adoptIfChanged = [](auto& cached, auto&& next) {
    using ValueT = typename std::decay_t<decltype(next)>::value_type;
    if (next.empty()) {
      cached = nullptr;
      return;
    }
    if (cached && *cached == next) {
      return;
    }
    cached = std::make_shared<const std::vector<ValueT>>(std::move(next));
  };

  std::vector<int> indices;
  std::vector<double> offsets;
  std::vector<double> sizes;
  std::size_t elementsSize = core.getElementsSize();
  // Sticky headers in an inverted list aren't supported. Publish nothing.

  if (!core.inverted) {
    for (std::size_t stickyIndex : core.stickyIndices) {
      if (stickyIndex >= elementsSize) {
        continue;
      }
      indices.push_back(static_cast<int>(stickyIndex));
      offsets.push_back(core.getElementOffset(stickyIndex));
      sizes.push_back(core.getElementSize(stickyIndex));
    }
  }
  adoptIfChanged(stickyHeaderIndices, std::move(indices));
  adoptIfChanged(stickyHeaderOffsets, std::move(offsets));
  adoptIfChanged(stickyHeaderSizes, std::move(sizes));
  // Empty unless snapToItem is set.
  adoptIfChanged(snapOffsets, std::vector<double>(core.getSnapOffsets()));
  return true;
}

}
