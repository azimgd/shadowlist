@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class TextInputTests: XCTestCase {
  func testProgrammaticValuesAreSilentAndUnchangedControlledRendersPreserveSelection() {
    let field = ShadowListUtilsTextInputField()
    let editing = ShadowListUtilsTextInput(input: .textField(field))
    var changes: [String] = []
    editing.onChangeText = { changes.append($0) }
    editing.value = "flight"
    let start = field.position(from: field.beginningOfDocument, offset: 2)!
    field.selectedTextRange = field.textRange(from: start, to: start)
    editing.value = "flight"
    XCTAssertEqual(field.offset(from: field.beginningOfDocument, to: field.selectedTextRange!.start), 2)
    XCTAssertEqual(changes, [])
    editing.value = "new flight"
    XCTAssertEqual(field.offset(from: field.beginningOfDocument, to: field.selectedTextRange!.start), 6)
    XCTAssertEqual(changes, [])
  }

  func testControlledEditsRequestOwnerChangesExactlyOnceAndRejectedEditsRestoreValue() {
    let field = ShadowListUtilsTextInputField()
    let editing = ShadowListUtilsTextInput(input: .textField(field))
    editing.value = "Saved"
    var changes: [String] = []
    editing.onChangeText = { changes.append($0) }
    field.text = "Candidate"
    editing.changed()
    XCTAssertEqual(changes, ["Candidate"])
    XCTAssertEqual(field.text, "Saved")
    editing.onChangeText = { changes.append($0); editing.value = $0 }
    field.text = "Accepted"
    editing.changed()
    XCTAssertEqual(changes, ["Candidate", "Accepted"])
    XCTAssertEqual(field.text, "Accepted")
  }

  func testNativeMarkedCompositionIsPreservedUntilTheRealUnmarkBoundary() {
    let field = ShadowListUtilsTextInputField()
    let editing = ShadowListUtilsTextInput(input: .textField(field))
    editing.value = ""
    var changes: [String] = []
    editing.onChangeText = { changes.append($0) }
    field.setMarkedText("に", selectedRange: NSRange(location: 1, length: 0))
    XCTAssertNotNil(field.markedTextRange)
    editing.changed()
    XCTAssertEqual(field.text, "に")
    field.unmarkText()
    XCTAssertNil(field.markedTextRange)
    XCTAssertEqual(field.text, "")
    XCTAssertEqual(changes, ["に"])
  }

  func testFieldAndMultilineViewUseTheSameComposedCharacterPasteLimit() {
    let field = ShadowListUtilsTextInputField()
    let fieldEditing = ShadowListUtilsTextInput(input: .textField(field))
    let view = ShadowListUtilsTextInputView()
    let viewEditing = ShadowListUtilsTextInput(input: .textView(view))
    fieldEditing.maxLength = 3
    viewEditing.maxLength = 3
    let paste = "AB👩‍👩‍👧‍👧C"
    XCTAssertFalse(fieldEditing.textField(field, shouldChangeCharactersIn: NSRange(location: 0, length: 0), replacementString: paste))
    XCTAssertFalse(viewEditing.textView(view, shouldChangeTextIn: NSRange(location: 0, length: 0), replacementText: paste))
    XCTAssertEqual(field.text, "AB")
    XCTAssertEqual(view.text, "AB")
  }

  func testSubmitAndClearAreSingleNativeCommandsWithoutInventingApplicationState() {
    let field = ShadowListUtilsTextInputField()
    let editing = ShadowListUtilsTextInput(input: .textField(field))
    editing.value = "Accepted"
    var changes: [String] = []
    var submitted = 0
    editing.onChangeText = { changes.append($0); editing.value = $0 }
    editing.onSubmitEditing = { submitted += 1 }
    XCTAssertTrue(editing.textFieldShouldReturn(field))
    XCTAssertEqual(submitted, 1)
    XCTAssertFalse(editing.textFieldShouldClear(field))
    XCTAssertEqual(changes, [""])
    XCTAssertEqual(editing.value, "")
  }
}
