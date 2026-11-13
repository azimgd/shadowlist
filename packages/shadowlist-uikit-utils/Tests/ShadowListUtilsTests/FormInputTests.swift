@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class FormInputTests: XCTestCase {
  func testLiteralKeyboardDefaultsAndExplicitOverrides() {
    let field = ShadowListUtilsFormInput(value: "12", accessibilityLabel: "Amount")
    XCTAssertEqual(field.input.autocapitalizationType, .sentences)
    XCTAssertEqual(field.input.autocorrectionType, .yes)
    field.numeric = true
    XCTAssertEqual(field.input.keyboardType, .decimalPad)
    XCTAssertEqual(field.input.autocapitalizationType, .none)
    XCTAssertEqual(field.input.autocorrectionType, .no)
    field.keyboardType = .emailAddress; field.autoCapitalize = .allCharacters; field.autoCorrect = true
    XCTAssertEqual(field.input.keyboardType, .emailAddress)
    XCTAssertEqual(field.input.autocapitalizationType, .allCharacters)
    XCTAssertEqual(field.input.autocorrectionType, .yes)
    XCTAssertEqual(field.accessibilityLabel, "Amount")
  }
  func testControlledEditAndSubmitPreserveApplicationOwnership() {
    let field = ShadowListUtilsFormInput(value: "before", accessibilityLabel: "Name")
    var changes: [String] = []
    field.onChange = { changes.append($0) }
    field.value = "programmatic"
    XCTAssertTrue(changes.isEmpty)
    field.input.text = "edit"
    // Logic tests have no UIApplication target dispatch; exercise the installed editing owner.
    // The native Components consumer separately validates actual keyboard/submit integration.
    field.input.editingOwner?.changed()
    XCTAssertEqual(changes, ["edit"])
    XCTAssertEqual(field.value, "programmatic")
    XCTAssertEqual(field.input.text, "programmatic")
    var submits = 0
    field.onSubmitEditing = { submits += 1 }
    _ = field.input.delegate?.textFieldShouldReturn?(field.input)
    XCTAssertEqual(submits, 1)
  }
  func testLargeTextUsesExistingPlatformScaleAndStacksTheForm() {
    let previous = ShadowListUtilsPlatform.textScaleOverride
    ShadowListUtilsPlatform.textScaleOverride = 1.5
    defer { ShadowListUtilsPlatform.textScaleOverride = previous }
    let field = ShadowListUtilsFormInput(value: "24", accessibilityLabel: "Amount")
    _ = ShadowListUtilsFormRow(label: "Amount", children: [field])
    XCTAssertTrue(field.stacked)
    XCTAssertEqual(field.input.font?.pointSize, field.theme.typography.body.fontSize * 1.5)
  }
  func testRowRetainsControlledFieldAndSupportsExplicitStacking() {
    let field = ShadowListUtilsFormInput(value: "24", accessibilityLabel: "Amount")
    let row = ShadowListUtilsFormRow(label: "Amount", children: [field])
    row.frame = CGRect(x: 0, y: 0, width: 240, height: 80)
    row.layoutIfNeeded()
    XCTAssertFalse(field.stacked)
    row.stacked = true; row.layoutIfNeeded()
    XCTAssertTrue(field.stacked)
    XCTAssertEqual(field.input.textAlignment, .left)
    row.children = [field]
    XCTAssertEqual(field.value, "24")
    XCTAssertNotNil(field.superview)
  }
}
