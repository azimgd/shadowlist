#pragma once

#include "CraigsListFenwickTree.hpp"
#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Size.h>

#ifdef ANDROID
#include <folly/dynamic.h>
#include <react/renderer/mapbuffer/MapBuffer.h>
#include <react/renderer/mapbuffer/MapBufferBuilder.h>
#endif

namespace facebook::react {

struct CraigsListContainerMetrics {
  int visibleStartIndex;
  int visibleEndIndex;
  
  double visibleStartPixels;
  double visibleEndPixels;
  
  int blankTopStartIndex;
  int blankTopEndIndex;
  
  double blankTopStartPixels;
  double blankTopEndPixels;
  
  int blankBottomStartIndex;
  int blankBottomEndIndex;
  
  double blankBottomStartPixels;
  double blankBottomEndPixels;
};

class CraigsListContainerState {
  public:
  CraigsListContainerState(
    Point scrollPosition,
    Size scrollContainer,
    Size scrollContent,
    CraigsListFenwickTree scrollContentTree);
  CraigsListContainerState() = default;

  /**
   * Keep track of container dimensions which a visible area,
   * content dimensions which is a long list from start to bottom,
   * and a scroll position which is an offset from top.
   */
  Point scrollPosition;
  Size scrollContainer;
  Size scrollContent;
  
  /**
   * Binary tree, expensive for updates, cheap for reads
   */
  CraigsListFenwickTree scrollContentTree;
  
  /**
   * Measure layout and children metrics
   */
  CraigsListContainerMetrics calculateLayoutMetrics(Point scrollPosition) const;
  float calculateItemOffset(int index) const;

#ifdef ANDROID
  CraigsListContainerState(CraigsListContainerState const &previousState, folly::dynamic data){};
  folly::dynamic getDynamic() const {
    return {};
  };
  MapBuffer getMapBuffer() const {
    return MapBufferBuilder::EMPTY();
  };
#endif

};

}
