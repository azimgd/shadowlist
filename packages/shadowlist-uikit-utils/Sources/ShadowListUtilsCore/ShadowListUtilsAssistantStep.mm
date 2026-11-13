#import "include/ShadowListUtilsCore/ShadowListUtilsAssistantStep.h"

#include <shadowlist-core/utils/AssistantStep.hpp>

@interface ShadowListUtilsAssistantTextStep ()
@property (nonatomic, copy) NSString *text;
@property (nonatomic) NSUInteger at;
@end

@interface ShadowListUtilsAssistantCallsStep ()
@property (nonatomic, copy) NSArray<NSNumber *> *callIndices;
@end

@implementation ShadowListUtilsAssistantStep
@end
@implementation ShadowListUtilsAssistantTextStep
@end
@implementation ShadowListUtilsAssistantCallsStep
@end

NSArray<ShadowListUtilsAssistantStep *> *ShadowListUtilsStepsOf(NSString *content, NSArray<NSNumber *> *positions)
{
  namespace sl = azimgd::shadowlist;
  std::vector<std::int64_t> offsets;
  offsets.reserve(positions.count);
  for (NSNumber *position in positions) offsets.push_back(position.longLongValue);
  NSData *utf8 = [content dataUsingEncoding:NSUTF8StringEncoding];
  const auto steps = sl::stepsOf(std::string_view(static_cast<const char *>(utf8.bytes), utf8.length), offsets);
  NSMutableArray *result = [NSMutableArray arrayWithCapacity:steps.size()];
  for (const auto& step : steps) {
    if (const auto *text = std::get_if<sl::AssistantTextStep>(&step)) {
      ShadowListUtilsAssistantTextStep *copy = [ShadowListUtilsAssistantTextStep new];
      copy.text = [[NSString alloc] initWithBytes:text->text.data() length:text->text.size() encoding:NSUTF8StringEncoding];
      copy.at = text->at;
      [result addObject:copy];
    } else {
      ShadowListUtilsAssistantCallsStep *copy = [ShadowListUtilsAssistantCallsStep new];
      NSMutableArray *indices = [NSMutableArray array];
      for (auto index : std::get<sl::AssistantCallsStep>(step).callIndices) [indices addObject:@(index)];
      copy.callIndices = indices;
      [result addObject:copy];
    }
  }
  return result;
}
