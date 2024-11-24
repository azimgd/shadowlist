#import <Foundation/Foundation.h>

#define SCROLLING_DOWN -1
#define SCROLLING_UP 1
#define SCROLLING_LEFT -1
#define SCROLLING_RIGHT 1

@interface SLScrollable : NSObject

- (void)updateState:(bool)horizontal
  inverted:(bool)inverted
  scrollContainerWidth:(float)scrollContainerWidth
  scrollContainerHeight:(float)scrollContainerHeight
  scrollContentWidth:(float)scrollContainerWidth
  scrollContentHeight:(float)scrollContainerHeight;
- (int)checkNotifyStart:(CGPoint)contentOffset;
- (int)checkNotifyEnd:(CGPoint)contentOffset;
- (int)shouldNotifyStart:(CGPoint)contentOffset;
- (int)shouldNotifyEnd:(CGPoint)contentOffset;
- (int)scrollDirectionHorizontal:(CGPoint)contentOffset;
- (int)scrollDirectionVertical:(CGPoint)contentOffset;

@end
