#include "CraigsListContainerState.h"

namespace facebook::react {

CraigsListContainerState::CraigsListContainerState(
  Point scrollPosition,
  Size scrollContainer,
  Size scrollContent,
  CraigsListFenwickTree scrollContentTree) :

    scrollPosition(scrollPosition),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    scrollContentTree(scrollContentTree) {}

}
