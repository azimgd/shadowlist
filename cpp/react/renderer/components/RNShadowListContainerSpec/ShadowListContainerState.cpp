#include "ShadowListContainerState.h"

namespace facebook::react {

ShadowListContainerState::ShadowListContainerState(
  Point scrollPosition,
  Size scrollContainer,
  Size scrollContent) :

    scrollPosition(scrollPosition),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent) {}
}
