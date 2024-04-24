#include "CraigsListContainerState.h"

namespace facebook::react {

CraigsListContainerState::CraigsListContainerState(Point scrollPosition, Size scrollContainer, Size scrollContent) :
  scrollPosition(scrollPosition),
  scrollContainer(scrollContainer),
  scrollContent(scrollContent) {}

}
