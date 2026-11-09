#pragma once

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

/*
 * Convert between keys the core holds and strings. A nil string becomes an empty key, and bytes
 * that are not UTF-8 become an empty string.
 */
NSString *ShadowListKitString(const std::string& value);
std::string ShadowListKitStdString(NSString *_Nullable string);

/*
 * The indices in a set, low to high.
 */
std::vector<std::size_t> ShadowListKitIndices(NSIndexSet *set);

NS_ASSUME_NONNULL_END
