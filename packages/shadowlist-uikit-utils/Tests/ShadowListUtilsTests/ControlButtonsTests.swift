@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class ControlButtonsTests: XCTestCase {
  func testPillVariantsAndDisabledActionAreControlled() {
    let button = ShadowListUtilsPillButton(label: "Continue")
    var presses = 0
    button.onPress = { presses += 1 }
    XCTAssertEqual(button.variant, .primary)
    XCTAssertEqual(button.backgroundColor, button.theme.colors.accent)
    let title = button.attributedTitle(for: .normal)!
    XCTAssertEqual(title.attribute(.kern, at: 0, effectiveRange: nil) as? CGFloat, button.theme.typography.body.letterSpacing * ShadowListUtilsPlatform.textScale)
    let paragraph = title.attribute(.paragraphStyle, at: 0, effectiveRange: nil) as! NSParagraphStyle
    XCTAssertEqual(paragraph.minimumLineHeight, button.theme.typography.body.scaledLineHeight())
    XCTAssertEqual(button.intrinsicContentSize.height, 40)
    XCTAssertGreaterThanOrEqual(button.intrinsicContentSize.width, 72)
    button.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 1)
    button.disabled = true
    button.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 1)
    XCTAssertEqual(button.alpha, 0.4, accuracy: 0.001)
    button.disabled = false; button.variant = .destructive
    XCTAssertEqual(button.titleColor(for: .normal), button.theme.colors.red)
    XCTAssertEqual(button.label, "Continue")
    XCTAssertEqual(button.accessibilityLabel, "Continue")
  }
  func testRippleThemeFallsBackToHighlightOnUIKitAndFormKeepsItsBehavior() {
    var theme = ShadowListUtilsTheme.light
    theme.pressFeedback = .ripple
    let pill = ShadowListUtilsPillButton(label: "Continue", theme: theme)
    pill.onPress = {}
    pill.isHighlighted = true
    XCTAssertEqual(pill.alpha, 0.55, accuracy: 0.001)
    let form = ShadowListUtilsFormButton(label: "Save", theme: theme)
    form.onPress = {}
    form.isHighlighted = true
    XCTAssertEqual(form.alpha, 0.55, accuracy: 0.001)
  }
  func testDashedButtonGrowsForLargeTextAndDisablesItsCallback() {
    let previous = ShadowListUtilsPlatform.textScaleOverride
    ShadowListUtilsPlatform.textScaleOverride = 1.5
    defer { ShadowListUtilsPlatform.textScaleOverride = previous }
    let button = ShadowListUtilsDashedButton(label: "Add another route with a long descriptive label")
    XCTAssertGreaterThan(button.sizeThatFits(CGSize(width: 150, height: 1000)).height, button.theme.tapTarget)
    var presses = 0
    button.onPress = { presses += 1 }; button.disabled = true
    button.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 0)
    XCTAssertEqual(button.alpha, 0.4, accuracy: 0.001)
  }
  func testIconAndBadgeKeepTheirNativeAccessibleIdentityAndFixedSizes() {
    let icon = UIImageView(frame: CGRect(x: 0, y: 0, width: 18, height: 18))
    let button = ShadowListUtilsIconButton(icon: icon, accessibilityLabel: "More routes")
    button.frame = CGRect(x: 0, y: 0, width: 40, height: 40); button.layoutIfNeeded()
    XCTAssertEqual(icon.frame.origin, CGPoint(x: 11, y: 11))
    XCTAssertFalse(icon.isAccessibilityElement)
    XCTAssertTrue(button.accessibilityTraits.contains(.button))
    XCTAssertEqual(button.accessibilityLabel, "More routes")
    XCTAssertTrue(button.point(inside: CGPoint(x: -3, y: 20), with: nil))
    XCTAssertFalse(button.point(inside: CGPoint(x: -5, y: 20), with: nil))
    let replacement = UIButton(type: .custom)
    replacement.frame = CGRect(x: 0, y: 0, width: 24, height: 24)
    replacement.isAccessibilityElement = true; replacement.accessibilityLabel = "Nested action"
    var outerPresses = 0; var nestedPresses = 0
    button.onPress = { outerPresses += 1 }
    replacement.addAction(UIAction { _ in nestedPresses += 1 }, for: .touchUpInside)
    button.icon = replacement; button.layoutIfNeeded()
    XCTAssertNil(icon.superview)
    XCTAssertTrue(replacement.superview === button)
    XCTAssertTrue(replacement.isUserInteractionEnabled)
    XCTAssertTrue(replacement.isAccessibilityElement)
    XCTAssertTrue(button.hitTest(CGPoint(x: 20, y: 20), with: nil) === replacement)
    replacement.sendActions(for: .touchUpInside)
    XCTAssertEqual(nestedPresses, 1); XCTAssertEqual(outerPresses, 0)
    XCTAssertTrue(button.hitTest(CGPoint(x: -3, y: 20), with: nil) === button)
    button.sendActions(for: .touchUpInside)
    XCTAssertEqual(outerPresses, 1)
    replacement.frame.size.width = 60
    button.setNeedsLayout(); button.layoutIfNeeded()
    XCTAssertTrue(button.hitTest(CGPoint(x: -3, y: 20), with: nil) === button)
    let badge = ShadowListUtilsIndexBadge(index: 0)
    XCTAssertEqual(badge.intrinsicContentSize, CGSize(width: 22, height: 22))
    XCTAssertEqual(badge.accessibilityLabel, "0")
    badge.index = 3
    XCTAssertEqual(badge.accessibilityLabel, "3")
  }
}
