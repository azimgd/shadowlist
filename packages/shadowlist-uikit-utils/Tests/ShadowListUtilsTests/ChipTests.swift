@testable import ShadowListUtils
import ShadowListUtilsCore
import UIKit
import XCTest

@MainActor
final class ChipTests: XCTestCase {
  private func content(_ chip: UIView) -> ShadowListUtilsChipContent { chip.subviews.first as! ShadowListUtilsChipContent }
  private func place(_ chip: UIView) {
    chip.frame = CGRect(origin: .zero, size: chip.intrinsicContentSize)
    chip.setNeedsLayout(); chip.layoutIfNeeded()
  }

  func testOptionalPressKeepsNormalAppearanceAndClearIsIndependent() {
    let chip = ShadowListUtilsChip(label: "Lisbon", theme: .light)
    var clears = 0; var presses = 0
    chip.onClear = { clears += 1 }
    let view = content(chip)
    XCTAssertTrue(view.button.isEnabled)
    view.button.isHighlighted = true
    XCTAssertEqual(view.alpha, 1)
    XCTAssertEqual(view.backgroundColor, chip.theme.colors.accentSoft)
    view.clearButton.sendActions(for: .touchUpInside)
    XCTAssertEqual(clears, 1); XCTAssertEqual(presses, 0); XCTAssertFalse(chip.selected)
    XCTAssertEqual(view.clearButton.accessibilityLabel, "Clear Lisbon")
    chip.onPress = { presses += 1 }
    view.button.isHighlighted = true; XCTAssertEqual(view.alpha, 0.55, accuracy: 0.001)
    view.button.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 1); XCTAssertEqual(clears, 1); XCTAssertFalse(chip.selected)
    chip.selected = true; chip.variant = .plain
    XCTAssertEqual(presses, 1); XCTAssertEqual(clears, 1)
    XCTAssertEqual(view.backgroundColor, chip.theme.colors.accent)
    XCTAssertTrue(view.button.accessibilityTraits.contains(.selected))
    chip.accessibilityLabel = "Destination"; chip.clearAccessibilityLabel = "Remove destination"
    XCTAssertEqual(view.button.accessibilityLabel, "Destination")
    XCTAssertEqual(view.clearButton.accessibilityLabel, "Remove destination")
  }

  func testClearHasItsOwnExpandedHitAndAccessibleAction() {
    let chip = ShadowListUtilsChip(label: "Lisbon")
    var presses = 0; var clears = 0
    chip.onPress = { presses += 1 }; chip.onClear = { clears += 1 }
    place(chip)
    let view = content(chip)
    let glyph = view.clearButton.titleLabel!.sizeThatFits(CGSize(width: 240, height: 1000))
    XCTAssertEqual(view.clearButton.frame.width, glyph.width)
    XCTAssertEqual(view.clearButton.frame.height, glyph.height)
    let point = CGPoint(x: view.clearButton.frame.minX - 6, y: view.bounds.midY)
    XCTAssertTrue(chip.hitTest(point, with: nil) === view.clearButton)
    (chip.hitTest(point, with: nil) as? UIControl)?.sendActions(for: .touchUpInside)
    XCTAssertEqual(clears, 1); XCTAssertEqual(presses, 0)
    XCTAssertEqual(view.accessibilityElements?.count, 2)
    XCTAssertTrue((view.accessibilityElements?[1] as? UIButton) === view.clearButton)
    chip.onClear = nil
    XCTAssertTrue(view.clearButton.isHidden); XCTAssertEqual(view.accessibilityElements?.count, 1)
  }

  func testFontCapMonoCompactAndMaximumWidthMatchTheLabelContract() {
    let previous = ShadowListUtilsPlatform.textScaleOverride
    ShadowListUtilsPlatform.textScaleOverride = 2
    defer { ShadowListUtilsPlatform.textScaleOverride = previous }
    let chip = ShadowListUtilsChip(label: String(repeating: "Destination ", count: 12))
    chip.onClear = {}; chip.maxFontSizeMultiplier = 1; chip.mono = true
    let view = content(chip)
    let title = view.button.attributedTitle(for: .normal)!
    XCTAssertEqual((title.attribute(.font, at: 0, effectiveRange: nil) as? UIFont)?.pointSize, 15)
    XCTAssertEqual(title.attribute(.kern, at: 0, effectiveRange: nil) as? CGFloat, -0.6)
    XCTAssertEqual(view.clearButton.titleLabel?.font.pointSize, 30)
    XCTAssertEqual(chip.intrinsicContentSize, CGSize(width: 240, height: 34))
    chip.size = .compact; XCTAssertEqual(chip.intrinsicContentSize.height, 28)
    chip.maxFontSizeMultiplier = 0
    let uncapped = view.button.attributedTitle(for: .normal)!
    XCTAssertEqual((uncapped.attribute(.font, at: 0, effectiveRange: nil) as? UIFont)?.pointSize, 26)
  }

  func testChoiceSelectionAndTintRemainControlledWithoutNewActions() {
    let chip = ShadowListUtilsChoiceChip(label: "Train", tint: .yellow, selected: false)
    var presses = 0; chip.onPress = { presses += 1 }
    let view = content(chip)
    view.button.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 1); XCTAssertFalse(chip.selected); XCTAssertNil(view.leadingView)
    chip.selected = true
    XCTAssertEqual(presses, 1); XCTAssertNotNil(view.leadingView)
    XCTAssertEqual(view.layer.borderWidth, 2)
    XCTAssertEqual(view.backgroundColor, .yellow)
    XCTAssertTrue(view.button.accessibilityTraits.contains(.selected))
    XCTAssertFalse(view.button.accessibilityTraits.contains(.notEnabled))
  }

  func testSharedWrapBridgePreservesExactFitAndVariableLineHeight() {
    let layout = ShadowListUtilsWrapLayout(width: 100, sizes: [CGSize(width: 40, height: 20), CGSize(width: 52, height: 34), CGSize(width: 30, height: 28)].map { NSValue(cgSize: $0) }, gap: 8)
    XCTAssertEqual(layout.frames.map(\.cgRectValue), [CGRect(x: 0, y: 0, width: 40, height: 20), CGRect(x: 48, y: 0, width: 52, height: 34), CGRect(x: 0, y: 42, width: 30, height: 28)])
    XCTAssertEqual(layout.height, 70)
  }

  func testRowMirrorsSharedFramesAndKeepsChildIdentityAcrossUpdates() {
    let previous = ShadowListUtilsPlatform.isRightToLeftOverride
    ShadowListUtilsPlatform.isRightToLeftOverride = false
    defer { ShadowListUtilsPlatform.isRightToLeftOverride = previous }
    let first = UIView(frame: CGRect(x: 0, y: 0, width: 40, height: 20))
    let second = UIView(frame: CGRect(x: 0, y: 0, width: 52, height: 34))
    let third = UIView(frame: CGRect(x: 0, y: 0, width: 30, height: 28))
    let row = ShadowListUtilsChipRow(children: [first, second, third], theme: .light)
    row.frame = CGRect(x: 0, y: 0, width: 100, height: 70); row.layoutIfNeeded()
    XCTAssertEqual(third.frame.minY, 42)
    XCTAssertEqual(row.sizeThatFits(CGSize(width: 100, height: 1000)).height, 70)
    ShadowListUtilsPlatform.isRightToLeftOverride = true
    row.setNeedsLayout(); row.layoutIfNeeded()
    XCTAssertEqual(first.frame.minX, 60); XCTAssertEqual(second.frame.minX, 0); XCTAssertEqual(third.frame.minX, 70)
    row.children = [third, first]; row.layoutIfNeeded()
    XCTAssertTrue(third.superview === row); XCTAssertNil(second.superview)
    row.children = []; XCTAssertNil(third.superview)
    XCTAssertEqual(row.sizeThatFits(CGSize(width: 100, height: 1000)).height, 0)
  }
}
