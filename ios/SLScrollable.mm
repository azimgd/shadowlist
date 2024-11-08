#import "SLScrollable.h"

@implementation SLScrollable {
  bool _horizontal;
  CGPoint _scrollContentOffset;
  float _visibleStartTrigger;
  float _visibleEndTrigger;
  float _scrollContainerWidth;
  float _scrollContainerHeight;
}

- (void)updateState:(bool)horizontal
  visibleStartTrigger:(float)visibleStartTrigger
  visibleEndTrigger:(float)visibleEndTrigger
  scrollContainerWidth:(float)scrollContainerWidth
  scrollContainerHeight:(float)scrollContainerHeight
{
  self->_visibleStartTrigger = visibleStartTrigger;
  self->_visibleEndTrigger = visibleEndTrigger;
  self->_scrollContainerWidth = scrollContainerWidth;
  self->_scrollContainerHeight = scrollContainerHeight;
}

- (bool)shouldUpdate:(CGPoint)contentOffset
{
  if (contentOffset.y < 0 || contentOffset.x < 0) {
    return true;
  }

  if (_horizontal) {
    if ([self scrollDirectionHorizontal:contentOffset] == SCROLLING_LEFT) {
      if (self->_visibleEndTrigger >= contentOffset.x + self->_scrollContainerWidth) {
        return false;
      }
    } else {
      if (self->_visibleStartTrigger <= contentOffset.x) {
        return false;
      }
    }
  } else {
    if ([self scrollDirectionVertical:contentOffset] == SCROLLING_DOWN) {
      if (self->_visibleEndTrigger >= contentOffset.y + self->_scrollContainerHeight) {
        return false;
      }
    } else {
      if (self->_visibleStartTrigger <= contentOffset.y) {
        return false;
      }
    }
  }
  return true;
}

- (int)scrollDirectionVertical:(CGPoint)contentOffset {
  int scrollDirection;
  if (contentOffset.y > self->_scrollContentOffset.y) {
    scrollDirection = SCROLLING_DOWN;
  } else {
    scrollDirection = SCROLLING_UP;
  }

  self->_scrollContentOffset = contentOffset;
  return scrollDirection;
}

- (int)scrollDirectionHorizontal:(CGPoint)contentOffset {
  int scrollDirection;
  if (contentOffset.x > self->_scrollContentOffset.x) {
    scrollDirection = SCROLLING_RIGHT;
  } else {
    scrollDirection = SCROLLING_LEFT;
  }

  self->_scrollContentOffset = contentOffset;
  return scrollDirection;
}

@end
