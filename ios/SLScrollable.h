#import <Foundation/Foundation.h>

#define SCROLLING_DOWN -1
#define SCROLLING_UP 1
#define SCROLLING_LEFT -1
#define SCROLLING_RIGHT 1

@interface SLScrollable : NSObject

- (void)updateState:(bool)horizontal
  visibleStartTrigger:(float)visibleStartTrigger
  visibleEndTrigger:(float)visibleEndTrigger
  scrollContainerWidth:(float)scrollContainerWidth
  scrollContainerHeight:(float)scrollContainerHeight;
- (bool)shouldUpdate:(CGPoint)contentOffset;
- (int)scrollDirectionHorizontal:(CGPoint)contentOffset;
- (int)scrollDirectionVertical:(CGPoint)contentOffset;

@end
