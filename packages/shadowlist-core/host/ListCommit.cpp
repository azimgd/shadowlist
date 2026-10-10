#include <shadowlist-core/host/ListCommit.hpp>

namespace azimgd::shadowlist {

namespace {

ScrollPhase scrollPhaseFromReport(double scrollPhase) {
  if (scrollPhase == SCROLL_PHASE_DRAGGING) {
    return ScrollPhase::Dragging;
  }
  if (scrollPhase == SCROLL_PHASE_SETTLING) {
    return ScrollPhase::Settling;
  }
  return ScrollPhase::Idle;
}

}

void applyHostScroll(FrameInput& input, const ListScrollState& state) {
  input.offsetX = state.offsetX;
  input.offsetY = state.offsetY;
  input.offsetEnabled = state.offsetEnabled;
  // A real user scroll drops any pending correction so the user isn't snapped back.
  input.userScrolled = state.userScrolled;
  // The phase lasts across reports, see Container::gestureActive.
  input.scrollPhase = scrollPhaseFromReport(state.scrollPhase);
  // The token the host echoed back. The core uses it to spot its own write. 0 if none.
  input.commitToken = state.commitToken;
}

bool publishStateUpdate(ListScrollState& state, const ContainerStateUpdate& update) {
  if (!update.changed) {
    return false;
  }
  bool continuesCorrection =
    update.commitToken != 0 && state.commitToken == update.commitToken;
  if (!continuesCorrection) {
    state.baseX = state.offsetX;
    state.baseY = state.offsetY;
  }
  state.offsetX = update.offsetX;
  state.offsetY = update.offsetY;
  state.contentWidth = update.contentWidth;
  state.contentHeight = update.contentHeight;
  state.offsetEnabled = update.applyOffset;
  // 0 when no offset was written.
  state.commitToken = update.commitToken;
  return true;
}

OffsetBand publishedOffsetBand(const Container& core, bool bandEnabled, bool rowsConcealed, bool measuringSizeSpecs) {
  if (!bandEnabled || rowsConcealed || measuringSizeSpecs) {
    return {};
  }
  return core.computeOffsetBand();
}

void stickyIndicesFromProps(const std::vector<int>& propIndices, std::vector<std::size_t>& indices) {
  indices.clear();
  for (int stickyHeaderIndex : propIndices) {
    if (stickyHeaderIndex >= 0) {
      indices.push_back(static_cast<std::size_t>(stickyHeaderIndex));
    }
  }
}

}
