#import "Internal/ShadowListKitItemSizeSpec.h"
#import "Internal/ShadowListKitPlatform.h"
#import "Internal/ShadowListKitCoreBridge.h"
#import <ShadowListKit/ShadowListKitText.h>
#import <CoreText/CoreText.h>
#include <cmath>

using namespace azimgd::shadowlist;

@implementation ShadowListKitItemSizeSpec
- (instancetype)initWithText:(NSString *)text
{
  if (self = [super init]) {
    _text = [text copy];
    _fontSize = RowSizeSpec::DEFAULT_FONT_SIZE;
    _lineHeight = NAN;
    _letterSpacing = NAN;
    _widthFraction = 1;
    _fixedHeight = NAN;
  }
  return self;
}
@end

RowSizeSpec ShadowListKitReadItemSizeSpec(ShadowListKitItemSizeSpec *value)
{
  RowSizeSpec spec;
  spec.text = ShadowListKitStdString(value.text);
  spec.fontFamily = ShadowListKitStdString(value.fontFamily);
  spec.fontWeight = ShadowListKitStdString(value.fontWeight);
  spec.fontStyle = ShadowListKitStdString(value.fontStyle);
  spec.fontSize = value.fontSize;
  spec.lineHeight = value.lineHeight;
  spec.letterSpacing = value.letterSpacing;
  spec.numberOfLines = (int)value.numberOfLines;
  spec.insetWidth = value.insetWidth;
  spec.insetHeight = value.insetHeight;
  spec.widthFraction = value.widthFraction;
  spec.fixedHeight = value.fixedHeight;
  return spec;
}

azimgd::shadowlist::Size ShadowListKitMeasureItemSizeSpec(const RowSizeSpec& spec, double width)
{
  if (!std::isnan(spec.fixedHeight)) return {width, spec.fixedHeight};
  double fraction = spec.widthFraction > 0 && spec.widthFraction <= 1 ? spec.widthFraction : 1;
  double textWidth = width * fraction - spec.insetWidth;
  if (textWidth <= 0) return {width, spec.insetHeight};
  NSString *family = spec.fontFamily.empty() ? nil : ShadowListKitString(spec.fontFamily);
  CGFloat fontSize = std::isfinite(spec.fontSize) && spec.fontSize > 0 ? spec.fontSize : RowSizeSpec::DEFAULT_FONT_SIZE;
  ShadowListKitFont *font = family ? [ShadowListKitFont fontWithName:family size:fontSize] : nil;
  if (!font) font = [ShadowListKitFont systemFontOfSize:fontSize];
  CTFontRef styled = (__bridge CTFontRef)font;
  CFRetain(styled);
  if (!spec.fontWeight.empty()) {
    NSString *weight = ShadowListKitString(spec.fontWeight);
    NSInteger numeric = [weight isEqualToString:@"bold"] ? 700 : [weight isEqualToString:@"normal"] ? 400 : weight.integerValue;
    NSArray<NSNumber *> *weights = @[@(-0.8), @(-0.6), @(-0.4), @0, @0.23, @0.3, @0.4, @0.56, @0.62];
    if (numeric < 100 || numeric > 900) numeric = 400;
    NSUInteger index = (NSUInteger)MAX(0, MIN(8, numeric / 100 - 1));
    NSDictionary *attributes = @{(__bridge NSString *)kCTFontTraitsAttribute: @{(__bridge NSString *)kCTFontWeightTrait: weights[index]}};
    CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes((__bridge CFDictionaryRef)attributes);
    CTFontRef weighted = CTFontCreateCopyWithAttributes(styled, fontSize, nullptr, descriptor);
    CFRelease(descriptor);
    CFRelease(styled);
    styled = weighted;
  }
  if (spec.fontStyle == "italic") {
    CTFontRef italic = CTFontCreateCopyWithSymbolicTraits(styled, fontSize, nullptr, kCTFontItalicTrait, kCTFontItalicTrait);
    if (italic) { CFRelease(styled); styled = italic; }
  }
  font = (__bridge ShadowListKitFont *)styled;
  CGFloat lineHeight = std::isfinite(spec.lineHeight) ? spec.lineHeight :
    std::ceil(CTFontGetAscent(styled) + CTFontGetDescent(styled) + CTFontGetLeading(styled));
  ShadowListKitTextLayout *layout = [ShadowListKitTextLayout layoutWithString:ShadowListKitString(spec.text)
    font:font lineHeight:lineHeight kern:std::isfinite(spec.letterSpacing) ? spec.letterSpacing : 0
    width:textWidth maximumLines:(NSUInteger)MAX(0, spec.numberOfLines) alignment:NSTextAlignmentNatural];
  double height = layout.size.height + spec.insetHeight;
  CFRelease(styled);
  return {width, height};
}
