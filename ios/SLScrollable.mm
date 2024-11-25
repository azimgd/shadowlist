#import "SLScrollable.h"

@implementation SLScrollable {
  bool _horizontal;
  bool _inverted;
  CGPoint _scrollContentOffset;
  float _scrollContainerWidth;
  float _scrollContainerHeight;
  float _scrollContentWidth;
  float _scrollContentHeight;
  float _lastContentOffsetX;
  float _lastContentOffsetY;
  float _startContentOffsetX;
  float _startContentOffsetY;
  NSTimeInterval _lastUpdateTime;
}

- (void)updateState:(bool)horizontal
  inverted:(bool)inverted
  scrollContainerWidth:(float)scrollContainerWidth
  scrollContainerHeight:(float)scrollContainerHeight
  scrollContentWidth:(float)scrollContentWidth
  scrollContentHeight:(float)scrollContentHeight
{
  self->_horizontal = horizontal;
  self->_inverted = inverted;
  self->_scrollContainerWidth = scrollContainerWidth;
  self->_scrollContainerHeight = scrollContainerHeight;
  self->_scrollContentWidth = scrollContentWidth;
  self->_scrollContentHeight = scrollContentHeight;
}

- (int)checkNotifyStart:(CGPoint)contentOffset
{
  if (self->_horizontal && contentOffset.x < self->_scrollContainerWidth) {
    return contentOffset.x;
  } else if (!self->_horizontal && contentOffset.y < self->_scrollContainerHeight) {
    return contentOffset.y;
  }
  return 0;
}

- (int)checkNotifyEnd:(CGPoint)contentOffset
{
  if (self->_horizontal && contentOffset.x > self->_scrollContentWidth - self->_scrollContainerWidth) {
    return ABS(self->_scrollContentWidth - contentOffset.x);
  } else if (!self->_horizontal && contentOffset.y > self->_scrollContentHeight - self->_scrollContainerHeight) {
    return ABS(self->_scrollContentHeight - contentOffset.y);
  }
  return 0;
}

- (int)shouldNotifyStart:(CGPoint)contentOffset
{
  if (!self->_inverted) {
    return [self checkNotifyStart:contentOffset];
  } else {
    return [self checkNotifyEnd:contentOffset];
  }
  return 0;
}

- (int)shouldNotifyEnd:(CGPoint)contentOffset
{
  if (!self->_inverted) {
    return [self checkNotifyEnd:contentOffset];
  } else {
    return [self checkNotifyStart:contentOffset];
  }
  return 0;
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

- (CGPoint)calculateVelocity {
  NSTimeInterval timeSinceLastUpdate = [NSDate timeIntervalSinceReferenceDate] - self->_lastUpdateTime;
  
  if (timeSinceLastUpdate > 0.01f) {
    CGFloat velocityX = (self->_lastContentOffsetX - self->_startContentOffsetX) / timeSinceLastUpdate;
    CGFloat velocityY = (self->_lastContentOffsetY - self->_startContentOffsetY) / timeSinceLastUpdate;

    self->_startContentOffsetX = self->_lastContentOffsetX;
    self->_startContentOffsetY = self->_lastContentOffsetY;
    
    self->_lastUpdateTime = [NSDate timeIntervalSinceReferenceDate];
    
    return CGPointMake(velocityX, velocityY);
  }
  
  return CGPointZero;
}

- (float)getScrollPosition:(CGPoint)scrollPosition {
  return self->_horizontal ? scrollPosition.x : scrollPosition.y;
}

- (float)getVisibleStartOffset:(CGPoint)scrollPosition {
  return [self getScrollPosition:scrollPosition];
}

- (float)getVisibleEndOffset:(CGPoint)scrollPosition {
  return [self getScrollPosition:scrollPosition] + self->_scrollContainerHeight;
}

- (CGPoint)getScrollPositionFromOffset:(float)scrollOffset {
  return self->_horizontal ? CGPointMake(scrollOffset, 0) : CGPointMake(0, scrollOffset);
}
@end
