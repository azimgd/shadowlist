import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * The model classes the AppKit kit shares with the UIKit kit.
 */
final class ModelsTests: XCTestCase {
  func testSwipeActionColorFollowsItsStyle() {
    let normal = ShadowListKitSwipeAction(style: .normal, title: "Flag") { _, completion in completion(true) }
    let destructive = ShadowListKitSwipeAction(style: .destructive, title: "Delete") { _, completion in completion(true) }
    XCTAssertEqual(normal.backgroundColor, NSColor.systemGray)
    XCTAssertEqual(destructive.backgroundColor, NSColor.systemRed)
    destructive.backgroundColor = .systemBlue
    XCTAssertEqual(destructive.backgroundColor, NSColor.systemBlue)
    destructive.backgroundColor = nil
    XCTAssertEqual(destructive.backgroundColor, NSColor.systemRed)
  }

  func testSwipeActionsConfigurationPerformsTheFirstActionByDefault() {
    let action = ShadowListKitSwipeAction(style: .normal, title: "Pin") { _, completion in completion(true) }
    let configuration = ShadowListKitSwipeActionsConfiguration(actions: [action])
    XCTAssertTrue(configuration.performsFirstActionWithFullSwipe)
    XCTAssertEqual(configuration.actions.count, 1)
  }

  func testAnchorStateSurvivesSecureCoding() throws {
    let state = ShadowListKitAnchorState(key: "row-\u{e9}-42", offset: 12.5)
    let data = try NSKeyedArchiver.archivedData(withRootObject: state, requiringSecureCoding: true)
    let decoded = try XCTUnwrap(NSKeyedUnarchiver.unarchivedObject(ofClass: ShadowListKitAnchorState.self, from: data))
    XCTAssertEqual(decoded.key, state.key)
    XCTAssertEqual(decoded.offset, state.offset)
  }
}
