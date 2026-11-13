@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class ActivityHeaderTests: XCTestCase {
  func testActionsWrapAndKeepTheirCallbacksAndAccessibleLabels() {
    var pressed = 0
    let actions = (0..<3).map { index in
      ShadowListUtilsActivityHeaderAction(label: "Long action \(index)", onPress: { pressed += 1 }, accessibilityLabel: "Action \(index)")
    }
    let header = ShadowListUtilsActivityHeader(title: "Activity", subtitle: "Your crew", actions: actions)
    let height = header.sizeThatFits(CGSize(width: 190, height: 1000)).height
    header.frame = CGRect(x: 0, y: 0, width: 190, height: height)
    header.layoutIfNeeded()
    let buttons = header.subviews.compactMap { $0 as? UIButton }
    XCTAssertEqual(buttons.count, 3)
    XCTAssertGreaterThan(buttons[1].frame.minY, buttons[0].frame.minY)
    XCTAssertEqual(buttons[0].accessibilityLabel, "Action 0")
    buttons[0].sendActions(for: .touchUpInside)
    XCTAssertEqual(pressed, 1)
    let plain = ShadowListUtilsActivityHeader(title: "Activity", subtitle: "Your crew")
    XCTAssertGreaterThan(height, plain.sizeThatFits(CGSize(width: 190, height: 1000)).height)
  }
}
