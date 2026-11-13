@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class SegmentedTests: XCTestCase {
  private let options = [ShadowListUtilsSegmentedOption(id: "plan", label: "Plan"), ShadowListUtilsSegmentedOption(id: "book", label: "Book"), ShadowListUtilsSegmentedOption(id: "travel", label: "Travel")]
  private func buttons(_ view: UIView) -> [ShadowListUtilsSegmentedButton] {
    view.subviews.flatMap { child in (child as? ShadowListUtilsSegmentedButton).map { [$0] } ?? buttons(child) }
  }
  private func place(_ view: UIView, width: CGFloat = 240) {
    view.frame = CGRect(x: 0, y: 0, width: width, height: view.intrinsicContentSize.height)
    view.setNeedsLayout(); view.layoutIfNeeded()
  }

  func testSelectionIsControlledAndRepeatedSelectedTapsStillNotify() {
    let control = ShadowListUtilsSegmented(options: options, value: "plan")
    var changes: [String] = []; control.onChange = { changes.append($0) }
    let cells = buttons(control)
    cells[0].sendActions(for: .touchUpInside); cells[0].sendActions(for: .touchUpInside)
    cells[1].sendActions(for: .touchUpInside)
    XCTAssertEqual(changes, ["plan", "plan", "book"]); XCTAssertEqual(control.value, "plan")
    control.value = "book"; XCTAssertEqual(changes.count, 3)
    XCTAssertFalse(cells[0].isSelected); XCTAssertTrue(cells[1].isSelected)
    control.value = "missing"; XCTAssertTrue(cells.allSatisfy { !$0.isSelected })
    control.options = []; place(control)
    XCTAssertTrue(buttons(control).isEmpty); XCTAssertEqual(control.intrinsicContentSize.height, 36)
  }

  func testDefaultLayoutDividersHighlightAndCompactMetrics() {
    let control = ShadowListUtilsSegmented(options: options, value: "plan", theme: .light)
    place(control)
    let cells = buttons(control)
    XCTAssertEqual(control.size, .regular); XCTAssertEqual(control.intrinsicContentSize.height, 36)
    XCTAssertEqual(cells[0].frame.width, cells[1].frame.width, accuracy: 0.01)
    XCTAssertTrue(cells[0].divider.isHidden); XCTAssertTrue(cells[1].divider.isHidden); XCTAssertFalse(cells[2].divider.isHidden)
    cells[0].isHighlighted = true; XCTAssertEqual(cells[0].alpha, 1)
    cells[2].isHighlighted = true; XCTAssertEqual(cells[2].alpha, 0.55, accuracy: 0.001)
    XCTAssertEqual(cells[0].layer.shadowOpacity, 0.1)
    control.size = .compact; place(control)
    XCTAssertEqual(control.intrinsicContentSize.height, 34)
    XCTAssertLessThan(control.intrinsicContentSize.width, 240)
    XCTAssertEqual(cells[0].label.attributedText?.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? UIColor, control.theme.colors.accent)
  }

  func testStableIDsKeepNativeViewsAndAccessibilityActivationReturnsCurrentID() {
    let control = ShadowListUtilsSegmented(options: options, value: "plan")
    let first = buttons(control)[0]
    var changes: [String] = []; control.onChange = { changes.append($0) }
    control.options = [options[2], ShadowListUtilsSegmentedOption(id: "plan", label: "Planning")]
    control.semanticContentAttribute = .forceLeftToRight; place(control)
    let current = buttons(control)
    XCTAssertTrue(current.first(where: { $0.option.id == "plan" }) === first)
    XCTAssertGreaterThan(first.frame.minX, current.first(where: { $0.option.id == "travel" })!.frame.minX)
    XCTAssertEqual(first.accessibilityLabel, "Planning")
    XCTAssertTrue(first.accessibilityTraits.contains(.selected))
    XCTAssertTrue(first.accessibilityActivate()); XCTAssertEqual(changes, ["plan"])
    XCTAssertEqual(control.value, "plan")
    control.accessibilityLabel = "Trip stage"; XCTAssertEqual(control.accessibilityLabel, "Trip stage")
  }

  func testRTLUsesNativeLayoutAndLargeTextDoesNotShrinkOrLoseTheFullLabel() {
    let previous = ShadowListUtilsPlatform.textScaleOverride
    ShadowListUtilsPlatform.textScaleOverride = 3
    defer { ShadowListUtilsPlatform.textScaleOverride = previous }
    let control = ShadowListUtilsSegmented(options: options, value: "plan")
    control.semanticContentAttribute = .forceRightToLeft; place(control)
    let cells = buttons(control)
    XCTAssertGreaterThan(cells[0].frame.minX, cells[2].frame.minX)
    XCTAssertEqual((cells[0].label.attributedText?.attribute(.font, at: 0, effectiveRange: nil) as? UIFont)?.pointSize, 39)
    XCTAssertGreaterThan(cells[0].label.frame.height, cells[0].bounds.height)
    XCTAssertEqual(cells[0].accessibilityLabel, "Plan")
    XCTAssertFalse(cells[0].clipsToBounds)
  }
}
