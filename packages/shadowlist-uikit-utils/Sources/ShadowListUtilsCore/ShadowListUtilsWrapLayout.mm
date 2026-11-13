#import "include/ShadowListUtilsCore/ShadowListUtilsWrapLayout.h"

#include <shadowlist-core/utils/WrapLayout.hpp>

@implementation ShadowListUtilsWrapLayout

- (instancetype)initWithWidth:(CGFloat)width sizes:(NSArray<NSValue *> *)sizes gap:(CGFloat)gap
{
  self = [super init];
  if (!self) return nil;
  std::vector<azimgd::shadowlist::Size> measured;
  measured.reserve(sizes.count);
  for (NSValue *value in sizes) {
    CGSize size;
    [value getValue:&size size:sizeof(size)];
    measured.push_back({size.width, size.height});
  }
  auto layout = azimgd::shadowlist::wrapLayout(width, measured, gap);
  NSMutableArray<NSValue *> *frames = [NSMutableArray arrayWithCapacity:layout.frames.size()];
  for (const auto& frame : layout.frames) {
    CGRect rect = CGRectMake(frame.x, frame.y, frame.width, frame.height);
    [frames addObject:[NSValue valueWithBytes:&rect objCType:@encode(CGRect)]];
  }
  _frames = [frames copy];
  _height = layout.height;
  return self;
}

@end
