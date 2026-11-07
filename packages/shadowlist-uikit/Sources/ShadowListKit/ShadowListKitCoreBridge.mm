#import "Internal/ShadowListKitCoreBridge.h"

#include <string>
#include <vector>

std::vector<std::size_t> ShadowListKitIndices(NSIndexSet *set)
{
  std::vector<NSUInteger> raw(set.count);
  [set getIndexes:raw.data() maxCount:raw.size() inIndexRange:nil];
  return std::vector<std::size_t>(raw.begin(), raw.end());
}

NSString *ShadowListKitString(const std::string& value)
{
  return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"";
}

/*
 * The string as UTF-8, embedded NULs kept. An ASCII string hands out its bytes without a copy.
 * Any other is converted straight into the result, with no autoreleased buffer in between.
 */
std::string ShadowListKitStdString(NSString *string)
{
  if (!string) {
    return std::string();
  }
  CFStringRef cfString = (__bridge CFStringRef)string;
  CFIndex length = CFStringGetLength(cfString);
  const char *ascii = CFStringGetCStringPtr(cfString, kCFStringEncodingASCII);
  if (ascii) {
    return std::string(ascii, (std::size_t)length);
  }
  std::string result([string maximumLengthOfBytesUsingEncoding:NSUTF8StringEncoding], '\0');
  NSUInteger used = 0;
  [string getBytes:result.data() maxLength:result.size() usedLength:&used encoding:NSUTF8StringEncoding
           options:NSStringEncodingConversionAllowLossy range:NSMakeRange(0, (NSUInteger)length) remainingRange:NULL];
  result.resize(used);
  return result;
}
