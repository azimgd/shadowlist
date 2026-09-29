#include <shadowlist-core/host/ListCommit.hpp>

namespace azimgd::shadowlist {

ScrollPhase scrollPhaseFromReport(double scrollPhase) {
  if (scrollPhase == SCROLL_PHASE_DRAGGING) {
    return ScrollPhase::Dragging;
  }
  if (scrollPhase == SCROLL_PHASE_SETTLING) {
    return ScrollPhase::Settling;
  }
  return ScrollPhase::Idle;
}

bool applyHostScroll(FrameInput& input, const ListScrollState& state, bool engineScrollCommand) {
  input.containerOffsetX = state.offsetX;
  input.containerOffsetY = state.offsetY;
  input.containerOffsetEnabled = state.offsetEnabled;
  /*
   * A real user scroll drops any pending correction so the user isn't snapped back.
   * Without it a correction can get stuck and freeze the window, leaving a blank list.
   */
  input.userScrolled = state.userScrolled;
  /*
   * The phase lasts across reports, so while a finger rests on the list the inverted bottom
   * pin doesn't pull the content under it. See Container::gestureActive.
   */
  input.scrollPhase = scrollPhaseFromReport(state.scrollPhase);
  // The token the host echoed back, so the core can spot its own write. 0 if none.
  input.commitToken = static_cast<std::uint64_t>(state.commitToken);

  bool yieldsMomentum = engineScrollCommand && input.scrollPhase != ScrollPhase::Dragging;
  if (yieldsMomentum) {
    input.userScrolled = false;
    input.scrollPhase = ScrollPhase::Idle;
  }
  return yieldsMomentum;
}

bool publishStateUpdate(ListScrollState& state, const ContainerStateUpdate& update, std::uint64_t engineYieldToken) {
  if (!update.changed) {
    return false;
  }
  bool continuesCorrection =
    update.commitToken != 0 && static_cast<std::uint64_t>(state.commitToken) == update.commitToken;
  if (!continuesCorrection) {
    state.baseX = state.offsetX;
    state.baseY = state.offsetY;
  }
  state.offsetX = update.containerOffsetX;
  state.offsetY = update.containerOffsetY;
  state.totalWidth = update.totalContainerWidth;
  state.totalHeight = update.totalContainerHeight;
  state.offsetEnabled = update.applyContainerOffset;
  // 0 when no offset was written.
  state.commitToken = static_cast<double>(update.commitToken);
  if (engineYieldToken != 0 && update.applyContainerOffset && update.commitToken == engineYieldToken) {
    state.momentumYieldToken = static_cast<double>(update.commitToken);
    state.userScrolled = false;
    state.scrollPhase = SCROLL_PHASE_IDLE;
  }
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
