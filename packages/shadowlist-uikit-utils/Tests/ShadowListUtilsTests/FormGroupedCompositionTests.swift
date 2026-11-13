@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class FormGroupedCompositionTests: XCTestCase {
  func testFormCardSeparatesRowsAndPreservesIdentityOnUpdate() {
    let first = ShadowListUtilsFormButton(label: "Save")
    let second = ShadowListUtilsFormButton(label: "Delete")
    let card = ShadowListUtilsFormCard(children: [first, second])
    let stack = card.subviews[0] as! UIStackView
    XCTAssertEqual(stack.arrangedSubviews.count, 3)
    XCTAssertTrue(stack.arrangedSubviews[0] === first)
    let separator = stack.arrangedSubviews[1]
    card.children = [first, second]
    XCTAssertTrue(stack.arrangedSubviews[1] === separator)
    card.children = [second]
    XCTAssertEqual(stack.arrangedSubviews.count, 1)
    XCTAssertTrue(stack.arrangedSubviews[0] === second)
    XCTAssertNil(first.superview)
  }
  func testFormValueAndButtonExposeApplicationStateAndActions() {
    let value = ShadowListUtilsFormValue(text: "")
    XCTAssertEqual(value.text, "")
    XCTAssertEqual(value.accessibilityLabel, "—")
    value.text = "€24"; value.accessibilityLabel = "Twenty four euros"
    XCTAssertEqual(value.accessibilityLabel, "Twenty four euros")
    let button = ShadowListUtilsFormButton(label: "Delete")
    XCTAssertFalse(button.isEnabled)
    var pressed = 0
    button.onPress = { pressed += 1 }; button.destructive = true
    button.sendActions(for: .touchUpInside)
    XCTAssertEqual(pressed, 1)
    XCTAssertEqual(button.titleColor(for: .normal), button.theme.colors.red)
    XCTAssertEqual(button.accessibilityLabel, "Delete")
    XCTAssertEqual(button.contentHorizontalAlignment, .center)
    button.isHighlighted = true
    XCTAssertEqual(button.alpha, 0.55, accuracy: 0.001)
    button.isHighlighted = false
    XCTAssertEqual(button.alpha, 1)
    XCTAssertGreaterThanOrEqual(button.sizeThatFits(CGSize(width: 180, height: 1000)).height, button.theme.tapTarget)
  }
  func testGroupedCompositionKeepsExplicitSeparatorsAndOptionalCaption() {
    let first = ShadowListUtilsFormValue(text: "Account")
    let separator = ShadowListUtilsGroupedSeparator(inset: 24)
    let card = ShadowListUtilsGroupedCard(children: [first, separator])
    let stack = card.subviews[0] as! UIStackView
    XCTAssertEqual(stack.arrangedSubviews.count, 2)
    card.children = [first, separator]
    XCTAssertTrue(stack.arrangedSubviews[0] === first)
    let section = ShadowListUtilsGroupedSection(children: [card])
    let sectionStack = section.subviews[0] as! UIStackView
    XCTAssertTrue(sectionStack.arrangedSubviews[0].isHidden)
    section.title = "Account"
    XCTAssertFalse(sectionStack.arrangedSubviews[0].isHidden)
    let caption = sectionStack.arrangedSubviews[0] as! ShadowListUtilsGroupedCaption
    let label = caption.subviews[0] as! UILabel
    XCTAssertTrue(label.accessibilityTraits.contains(.header))
    section.title = ""
    XCTAssertTrue(caption.isHidden)
  }
  func testSeparatorHasOneHairlineAndNoAccessibilityStop() {
    let line = ShadowListUtilsGroupedSeparator(inset: 30)
    line.frame = CGRect(x: 0, y: 0, width: 200, height: ShadowListUtilsTheme.hairline)
    line.layoutIfNeeded()
    XCTAssertEqual(line.subviews[0].frame.width, 170)
    XCTAssertEqual(line.subviews[0].frame.height, ShadowListUtilsTheme.hairline)
    XCTAssertFalse(line.isAccessibilityElement)
  }
}
