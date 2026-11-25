#import <ShadowListKit/ShadowListKitTreeRows.h>

#include <shadowlist-core/host/TreeSource.hpp>

#include <string>
#include <vector>

namespace sl = azimgd::shadowlist;

@implementation ShadowListKitTreeRow

- (instancetype)initWithKey:(NSString *)key
                      depth:(NSUInteger)depth
                hasChildren:(BOOL)hasChildren
                   expanded:(BOOL)expanded
{
  if ((self = [super init])) {
    _key = [key copy];
    _depth = depth;
    _hasChildren = hasChildren;
    _expanded = expanded;
  }
  return self;
}

@end

@implementation ShadowListKitTreeRows {
  sl::TreeSource _source;
}

- (void)resetRows:(NSArray<ShadowListKitTreeRow *> *)rows
{
  std::vector<sl::TreeRow> encoded;
  encoded.reserve(rows.count);
  for (ShadowListKitTreeRow *row in rows) {
    encoded.push_back(
      {std::string(row.key.UTF8String), static_cast<std::uint32_t>(row.depth), row.hasChildren, row.isExpanded});
  }
  _source.reset(std::move(encoded));
}

- (void)setExpandedKeys:(NSArray<NSString *> *)keys
{
  std::vector<std::string> encoded;
  encoded.reserve(keys.count);
  for (NSString *key in keys) {
    encoded.emplace_back(key.UTF8String);
  }
  _source.setExpandedKeys(encoded);
}

- (NSArray<ShadowListKitTreeRow *> *)rows
{
  NSMutableArray<ShadowListKitTreeRow *> *rows = [NSMutableArray arrayWithCapacity:_source.rows().size()];
  for (std::size_t index = 0; index < _source.rows().size(); ++index) {
    auto row = _source.rows().rowAt(index);
    [rows addObject:[[ShadowListKitTreeRow alloc] initWithKey:@(row.key.c_str())
                                                        depth:row.depth
                                                  hasChildren:row.hasChildren
                                                     expanded:row.expanded]];
  }
  return rows;
}

- (NSArray<NSString *> *)expandedKeys
{
  NSMutableArray<NSString *> *keys = [NSMutableArray array];
  for (const auto& key : _source.expandedKeys()) {
    [keys addObject:@(key.c_str())];
  }
  return keys;
}

- (void)toggleKey:(NSString *)key
{
  _source.toggle(std::string(key.UTF8String));
}

- (void)expandRow:(NSUInteger)index
{
  _source.expand(index);
}
- (void)collapseRow:(NSUInteger)index
{
  _source.collapse(index);
}

@end
