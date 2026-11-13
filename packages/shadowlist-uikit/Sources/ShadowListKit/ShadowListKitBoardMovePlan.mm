#import <ShadowListKit/ShadowListKitBoardMovePlan.h>

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/host/DragBoard.hpp>

namespace sl = azimgd::shadowlist;

@interface ShadowListKitBoardMovePlan ()
- (instancetype)initResolved;
@end

@implementation ShadowListKitBoardMovePlan

- (instancetype)initResolved { return [super init]; }

+ (instancetype)planWithColumns:(NSArray<NSArray<NSString *> *> *)columns itemKey:(NSString *)itemKey
             destinationColumn:(NSUInteger)destinationColumn destinationKey:(NSString *)destinationKey
              destinationIndex:(NSUInteger)destinationIndex {
  std::vector<std::vector<std::string>> encoded;
  encoded.reserve(columns.count);
  for (NSArray<NSString *> *column in columns) {
    std::vector<std::string> keys;
    keys.reserve(column.count);
    for (NSString *key in column) keys.emplace_back(key.UTF8String);
    encoded.push_back(std::move(keys));
  }
  auto plan = sl::planBoardMove(encoded, std::string(itemKey.UTF8String),
    destinationColumn == NSNotFound ? sl::UNDEFINED_INDEX : destinationColumn,
    std::string(destinationKey.UTF8String), destinationIndex == NSNotFound ? sl::UNDEFINED_INDEX : destinationIndex);
  ShadowListKitBoardMovePlan *result = [[self alloc] initResolved];
  result->_sourceColumn = plan.sourceColumn == sl::UNDEFINED_INDEX ? NSNotFound : plan.sourceColumn;
  result->_sourceIndex = plan.sourceIndex == sl::UNDEFINED_INDEX ? NSNotFound : plan.sourceIndex;
  result->_destinationColumn = plan.destinationColumn == sl::UNDEFINED_INDEX ? NSNotFound : plan.destinationColumn;
  result->_destinationIndex = plan.destinationIndex == sl::UNDEFINED_INDEX ? NSNotFound : plan.destinationIndex;
  result->_empty = plan.isEmpty();
  return result;
}

@end
