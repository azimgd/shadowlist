#import <AppKit/AppKit.h>
#import <XCTest/XCTest.h>

/*
 * Exercise the mounted Fabric component without depending on private pod headers.
 */
@protocol ShadowListTestCommands
- (void)scrollToItem:(NSInteger)index viewPosition:(double)position;
- (void)scrollToEnd:(BOOL)animated;
@end

static NSView *FindView(NSView *root, Class type)
{
  if ([root isKindOfClass:type]) {
    return root;
  }
  for (NSView *child in root.subviews) {
    NSView *found = FindView(child, type);
    if (found) {
      return found;
    }
  }
  return nil;
}

@interface ShadowListMacOSTests : XCTestCase
@end

@implementation ShadowListMacOSTests
- (NSView<ShadowListTestCommands> *)mountedList
{
  for (NSWindow *window in NSApp.windows) {
    NSView *list = FindView(window.contentView, NSClassFromString(@"ShadowListView"));
    if (list) {
      return (NSView<ShadowListTestCommands> *)list;
    }
  }
  return nil;
}

- (void)waitFor:(BOOL (^)(void))condition
{
  NSPredicate *predicate = [NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
    return condition();
  }];
  XCTestExpectation *expectation = [self expectationForPredicate:predicate evaluatedWithObject:self handler:nil];
  [self waitForExpectations:@[expectation] timeout:20];
}

- (void)testVirtualizationAndScrollCommands
{
  [self waitFor:^BOOL {
    NSScrollView *scroll = (NSScrollView *)FindView([self mountedList], NSScrollView.class);
    return scroll.documentView.frame.size.height > 10000 && scroll.documentView.subviews.count > 0;
  }];
  NSView<ShadowListTestCommands> *list = [self mountedList];
  NSScrollView *scroll = (NSScrollView *)FindView(list, NSScrollView.class);
  XCTAssertNotNil(list);
  XCTAssertGreaterThan(scroll.contentView.bounds.size.width, 100);
  XCTAssertEqualWithAccuracy(scroll.documentView.frame.size.width, list.bounds.size.width, 1);
  XCTAssertLessThan(scroll.documentView.subviews.count, 200u); // The example contains 1000 rows.

  [list scrollToItem:500 viewPosition:0];
  [self waitFor:^BOOL { return scroll.documentVisibleRect.origin.y > 30000; }];
  XCTAssertLessThan(scroll.documentView.subviews.count, 200u);

  [list scrollToEnd:NO];
  [self waitFor:^BOOL {
    CGFloat bottom = NSMaxY(scroll.documentVisibleRect);
    return fabs(bottom - scroll.documentView.frame.size.height) < 2;
  }];

  [list scrollToItem:0 viewPosition:0];
  [self waitFor:^BOOL { return scroll.documentVisibleRect.origin.y < 100; }];
}

- (void)testResizeKeepsDocumentWidthAndVirtualization
{
  [self waitFor:^BOOL { return [self mountedList].bounds.size.width > 100; }];
  NSView *list = [self mountedList];
  NSWindow *window = list.window;
  NSSize previous = window.contentView.frame.size;
  [window setContentSize:NSMakeSize(900, 600)];
  [self waitFor:^BOOL {
    NSScrollView *scroll = (NSScrollView *)FindView(list, NSScrollView.class);
    return list.bounds.size.width < 900 &&
      fabs(scroll.documentView.frame.size.width - list.bounds.size.width) < 1;
  }];
  NSScrollView *scroll = (NSScrollView *)FindView(list, NSScrollView.class);
  XCTAssertGreaterThan(scroll.documentView.subviews.count, 0u);
  XCTAssertLessThan(scroll.documentView.subviews.count, 200u);
  [window setContentSize:previous];
}

static NSView *FindFirst(NSView *root, BOOL (^match)(NSView *))
{
  if (match(root)) {
    return root;
  }
  for (NSView *child in root.subviews) {
    NSView *found = FindFirst(child, match);
    if (found) {
      return found;
    }
  }
  return nil;
}

/*
 * Declared instead of imported: the renderer adds this category to NSColor, and this target
 * reaches the app only through selectors rather than private pod headers.
 */
@interface NSColor (SLAppearanceResolving)
- (NSColor *)resolvedColorWithAppearance:(NSAppearance *)appearance;
@end

/*
 * The theme hands the renderer AppKit semantic colors rather than hex values. That only pays
 * off if a color survives the trip to the native view still dynamic. The test resolves a
 * mounted view's own background under each appearance. A hex that made it this far would resolve to
 * the same value twice and fail below.
 */
static NSColor *ResolveUnderAppearance(NSColor *color, NSAppearanceName appearanceName)
{
  return [[color resolvedColorWithAppearance:[NSAppearance appearanceNamed:appearanceName]]
      usingColorSpace:NSColorSpace.deviceRGBColorSpace];
}

/*
 * Reads a catalog color through its selector inside the given appearance. The expectation then
 * holds whichever appearance the test host itself draws in.
 */
static NSColor *CatalogColorUnderAppearance(NSString *name, NSAppearanceName appearanceName)
{
  __block NSColor *resolved = nil;
  [[NSAppearance appearanceNamed:appearanceName] performAsCurrentDrawingAppearance:^{
    SEL selector = NSSelectorFromString(name);
    NSInvocation *invocation =
        [NSInvocation invocationWithMethodSignature:[NSColor methodSignatureForSelector:selector]];
    invocation.target = NSColor.class;
    invocation.selector = selector;
    [invocation invoke];
    __unsafe_unretained NSColor *color = nil;
    [invocation getReturnValue:&color];
    resolved = [color usingColorSpace:NSColorSpace.deviceRGBColorSpace];
  }];
  return resolved;
}

static BOOL SameColor(NSColor *left, NSColor *right)
{
  if (left == nil || right == nil) {
    return NO;
  }
  for (NSInteger channel = 0; channel < 4; channel++) {
    if (fabs([left colorComponentAtIndex:channel] - [right colorComponentAtIndex:channel]) >
        1.0 / 255.0) {
      return NO;
    }
  }
  return YES;
}

- (void)testThemeColorsResolvePerAppearance
{
  [self waitFor:^BOOL { return [self mountedList] != nil; }];
  Class viewComponent = NSClassFromString(@"RCTViewComponentView");
  XCTAssertNotNil(viewComponent);

  NSView *root = [NSApp.windows firstObject].contentView;
  NSView *probe = FindFirst(root, ^BOOL(NSView *view) {
    if (![view isKindOfClass:viewComponent]) {
      return NO;
    }
    NSColor *color = [view valueForKey:@"backgroundColor"];
    return [color isKindOfClass:NSColor.class] && color.isDynamic;
  });
  XCTAssertNotNil(probe, @"No mounted view kept a dynamic AppKit color.");

  NSColor *color = [probe valueForKey:@"backgroundColor"];
  XCTAssertTrue(color.isDynamic);
  XCTAssertFalse(SameColor(ResolveUnderAppearance(color, NSAppearanceNameAqua),
                           ResolveUnderAppearance(color, NSAppearanceNameDarkAqua)),
                 @"A dynamic color that resolves the same light and dark is a hex in disguise.");

  /*
   * The color must land on the AppKit color the theme named, not merely on some color that
   * happens to differ between appearances.
   */
  XCTAssertTrue(SameColor(ResolveUnderAppearance(color, NSAppearanceNameAqua),
                          CatalogColorUnderAppearance(@"windowBackgroundColor", NSAppearanceNameAqua)),
                @"The window background did not resolve to windowBackgroundColor.");
  XCTAssertTrue(SameColor(ResolveUnderAppearance(color, NSAppearanceNameDarkAqua),
                          CatalogColorUnderAppearance(@"windowBackgroundColor", NSAppearanceNameDarkAqua)),
                @"The window background did not track windowBackgroundColor into Dark Mode.");
}
@end
