#include <shadowlist-core/host/ListLayout.hpp>

#include <shadowlist-core/Virtualizer.hpp>

#include <limits>
#include <type_traits>
#include <utility>

namespace azimgd::shadowlist {

bool applyLayoutInputs(Container& core, double headerSize, double footerSize, double windowWidth, double windowHeight) {
  bool changed = core.headerSize != headerSize || core.footerSize != footerSize ||
    core.revision.windowWidth != windowWidth || core.revision.windowHeight != windowHeight;
  if (!changed) {
    return false;
  }
  double previousHeaderSize = core.headerSize;
  double previousWindowSize = core.getWindowSize();
  // Row offsets only depend on the header and the window's cross size.
  bool rowsMove = previousHeaderSize != headerSize ||
    (core.horizontal ? core.revision.windowHeight != windowHeight
                     : core.revision.windowWidth != windowWidth);
  core.headerSize = headerSize;
  core.footerSize = footerSize;
  core.revision.windowWidth = windowWidth;
  core.revision.windowHeight = windowHeight;
  if (rowsMove) {
    Virtualizer::recomputeRowOffsets(core, 0);
    /*
     * The next layout has nothing left to reflow for these inputs. The measured range still
     * comes from before they changed, and the next frame picks it again.
     */
    core.previousLayoutHeaderSize = headerSize;
    core.previousLayoutWindowWidth = windowWidth;
    core.previousLayoutWindowHeight = windowHeight;
    core.measuredRangeStale = true;
  }
  // The core's frame ran with the previous header size. Settle the change now.
  Virtualizer::applyHeaderSizeChange(core, previousHeaderSize);
  // A chat resting at its bottom keeps it as the composer resizes the list.
  Virtualizer::applyWindowSizeChange(core, previousWindowSize);
  // While the user is scrolled this just writes the current offset again.
  core.offsetCorrected = true;
  return true;
}

bool SizeBatch::apply(Container& core, std::size_t index, Size size) {
  bool changed = Virtualizer::applyRowSize(core, index, size);
  if (changed && index < lowestChangedIndex_) {
    lowestChangedIndex_ = index;
  }
  return changed;
}

bool SizeBatch::commit(Container& core) {
  bool changed = lowestChangedIndex_ != UNDEFINED_INDEX;
  if (changed) {
    Virtualizer::commitRowSizes(core, lowestChangedIndex_);
  }
  Virtualizer::recomputeContentSize(core);
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
    const Row& placed = core.getRowAtIndex(row.index);
    Size size{row.width, row.height};
    if (core.numberOfColumns > 1) {
      if (horizontal) {
        size.height = placed.height;
      } else {
        size.width = placed.width;
      }
    }
    if (!placed.measured) {
      firstMeasured.push_back(row.id);
    }
    batch.apply(core, row.index, size);
  }
  batch.commit(core);
}

RowFrame rowFrame(const Container& core, std::size_t index, bool horizontal) {
  const Row& row = core.getRowAtIndex(index);
  RowFrame frame;
  if (core.numberOfColumns > 1) {
    frame.x = row.offsetX;
    frame.y = row.offsetY;
    frame.width = row.width;
    frame.setsWidth = true;
  } else if (horizontal) {
    frame.x = row.offsetX;
  } else {
    frame.y = row.offsetY;
  }
  return frame;
}

TemplateOffsets templateOffsets(const Container& core, double headerSize, double footerSize) {
  TemplateOffsets offsets;
  offsets.empty = headerSize;
  offsets.footer = core.getFooterStart(footerSize);
  return offsets;
}

bool PublishedGeometry::refresh(const Container& core) {
  double windowSize = core.getWindowSize();
  double totalSize = core.horizontal ? core.revision.contentWidth : core.revision.contentHeight;
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
    cached = std::make_shared<const std::vector<ValueT>>(std::forward<decltype(next)>(next));
  };

  std::vector<int> indices;
  std::vector<double> offsets;
  std::vector<double> sizes;
  std::size_t rowCount = core.getRowCount();
  // Sticky headers in an inverted list aren't supported. Publish nothing.

  if (!core.inverted) {
    for (std::size_t stickyIndex : core.stickyIndices) {
      // The published indices are ints. A row past that range is never pinned.
      if (stickyIndex >= rowCount || stickyIndex > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        continue;
      }
      indices.push_back(static_cast<int>(stickyIndex));
      offsets.push_back(core.getRowOffset(stickyIndex));
      sizes.push_back(core.getRowSize(stickyIndex));
    }
  }
  adoptIfChanged(stickyIndices, std::move(indices));
  adoptIfChanged(stickyOffsets, std::move(offsets));
  adoptIfChanged(stickySizes, std::move(sizes));
  // Empty unless snapToItem is set. Compared before it is copied.
  adoptIfChanged(snapOffsets, core.getSnapOffsets());
  return true;
}

}
