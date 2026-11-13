@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class FieldTests: XCTestCase {
  func testTextFieldModeChangePreservesControlledValueAndSilence() {
    let field = ShadowListUtilsTextField(value: "first\nsecond")
    var changes: [String] = []
    field.onChange = { changes.append($0) }
    field.value = "updated"
    XCTAssertTrue(changes.isEmpty)
    field.multiline = true
    XCTAssertEqual(field.textView.text, "updated")
    XCTAssertEqual(field.textView.attributedText.attribute(.kern, at: 0, effectiveRange: nil) as? CGFloat, -0.6 * ShadowListUtilsPlatform.textScale)
    XCTAssertEqual(field.intrinsicContentSize.height, 104)
    field.multiline = false
    field.placeholder = "Notes"
    XCTAssertEqual(field.field.accessibilityLabel, "Notes")
    field.accessibilityLabel = "Travel notes"
    field.placeholder = "New hint"
    XCTAssertEqual(field.field.accessibilityLabel, "Travel notes")
    XCTAssertEqual(field.field.text, "updated")
    XCTAssertEqual(field.intrinsicContentSize.height, 40)
    XCTAssertTrue(changes.isEmpty)
  }
  func testSearchLabelsAndCustomIconPreserveCallerSemantics() {
    let field = ShadowListUtilsSearchField(value: "Lisbon")
    var changes: [String] = []
    field.onChange = { changes.append($0) }
    field.value = "Porto"
    var labels = ShadowListUtilsSearchFieldLabels()
    labels.placeholder = "Find route"
    field.labels = labels
    XCTAssertEqual(field.input.text, "Porto")
    XCTAssertEqual(field.input.accessibilityLabel, "Find route")
    XCTAssertEqual(field.input.clearButtonMode, .always)
    XCTAssertTrue(changes.isEmpty)
    let icon = UIButton(type: .system)
    icon.translatesAutoresizingMaskIntoConstraints = false
    NSLayoutConstraint.activate([icon.widthAnchor.constraint(equalToConstant: 32), icon.heightAnchor.constraint(equalToConstant: 24)])
    icon.isAccessibilityElement = true; icon.accessibilityLabel = "Filter options"
    var presses = 0
    icon.addAction(UIAction { _ in presses += 1 }, for: .touchUpInside)
    field.icon = icon
    XCTAssertTrue(icon.superview === field)
    XCTAssertTrue(icon.isAccessibilityElement)
    XCTAssertTrue(icon.isUserInteractionEnabled)
    icon.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 1)
    XCTAssertEqual(icon.accessibilityLabel, "Filter options")
    field.frame = CGRect(x: 0, y: 0, width: 200, height: 36); field.layoutIfNeeded()
    XCTAssertEqual(icon.frame.size, CGSize(width: 32, height: 24))
    XCTAssertEqual(field.input.frame.minX, 48)
    field.icon = nil; field.layoutIfNeeded()
    XCTAssertNil(icon.superview)
    XCTAssertEqual(field.input.frame.minX, 32)
    let defaultIcon = field.subviews.first { $0 is UIImageView }
    XCTAssertEqual(defaultIcon?.frame.size, CGSize(width: 16, height: 16))
    XCTAssertFalse(defaultIcon?.isAccessibilityElement ?? true)
    XCTAssertFalse(defaultIcon?.isUserInteractionEnabled ?? true)
  }
}
