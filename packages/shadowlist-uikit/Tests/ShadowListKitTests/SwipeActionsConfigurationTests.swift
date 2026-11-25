import XCTest
#if canImport(UIKit)
  import ShadowListKit
  import UIKit

  private typealias ActionColor = UIColor
#else
  import AppKit
  import ShadowListKitAppKit

  private typealias ActionColor = NSColor
#endif

@MainActor
final class SwipeActionsConfigurationTests: XCTestCase {
  func testDefaultColorCopyPreservesExplicitColorsAndCallerConfiguration() {
    let unspecified = ShadowListKitSwipeAction(style: .destructive, title: "Delete") { _, _ in }
    let explicitRed = ShadowListKitSwipeAction(style: .destructive, title: "Red") { _, _ in }
    explicitRed.backgroundColor = .systemRed
    let explicitClear = ShadowListKitSwipeAction(style: .destructive, title: "Clear") { _, _ in }
    explicitClear.backgroundColor = .clear
    let normal = ShadowListKitSwipeAction(style: .normal, title: "Archive") { _, _ in }
    let original = ShadowListKitSwipeActionsConfiguration(actions: [unspecified, explicitRed, explicitClear, normal])
    original.performsFirstActionWithFullSwipe = false
    let themed = original.applying(defaultDestructiveColor: .purple)
    XCTAssertFalse(themed === original)
    XCTAssertFalse(themed.actions[0] === unspecified)
    XCTAssertEqual(themed.actions[0].backgroundColor, ActionColor.purple)
    XCTAssertTrue(themed.actions[1] === explicitRed)
    XCTAssertEqual(themed.actions[1].backgroundColor, ActionColor.systemRed)
    XCTAssertTrue(themed.actions[2] === explicitClear)
    XCTAssertEqual(themed.actions[2].backgroundColor, ActionColor.clear)
    XCTAssertTrue(themed.actions[3] === normal)
    XCTAssertFalse(themed.performsFirstActionWithFullSwipe)
    XCTAssertEqual(unspecified.backgroundColor, ActionColor.systemRed)
    XCTAssertEqual(normal.backgroundColor, ActionColor.systemGray)
    let nextTheme = original.applying(defaultDestructiveColor: .orange)
    XCTAssertEqual(nextTheme.actions[0].backgroundColor, ActionColor.orange)
    XCTAssertEqual(themed.actions[0].backgroundColor, ActionColor.purple)
    unspecified.backgroundColor = .clear
    XCTAssertEqual(original.applying(defaultDestructiveColor: .orange).actions[0].backgroundColor, ActionColor.clear)
    unspecified.backgroundColor = nil
    XCTAssertEqual(original.applying(defaultDestructiveColor: .orange).actions[0].backgroundColor, ActionColor.orange)
  }

  func testCopyPreservesImageTitleHandlerAndFullSwipe() {
    var calledAction: ShadowListKitSwipeAction?
    var performed = false
    let action = ShadowListKitSwipeAction(style: .destructive, title: "Delete") { action, completion in
      calledAction = action
      completion(true)
    }
    #if canImport(UIKit)
      let image = UIImage()
    #else
      let image = NSImage(size: NSSize(width: 10, height: 10))
    #endif
    action.image = image
    let original = ShadowListKitSwipeActionsConfiguration(actions: [action])
    let themed = original.applying(defaultDestructiveColor: .purple)
    XCTAssertTrue(themed.performsFirstActionWithFullSwipe)
    let copiedAction = themed.actions[0]
    XCTAssertEqual(copiedAction.title, "Delete")
    XCTAssertTrue(copiedAction.image === image)
    XCTAssertEqual(copiedAction.style, .destructive)
    copiedAction.handler(copiedAction) { performed = $0 }
    XCTAssertTrue(calledAction === copiedAction)
    XCTAssertTrue(performed)
    XCTAssertEqual(action.backgroundColor, ActionColor.systemRed)
  }
}
