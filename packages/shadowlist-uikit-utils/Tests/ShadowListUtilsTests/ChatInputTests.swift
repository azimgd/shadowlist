@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class ChatInputTests: XCTestCase {
  func testAnUncontrolledDraftTrimsSendsAndClearsWithoutChangingTheCallback() {
    let composer = ShadowListUtilsChatInput(defaultValue: "  Hello crew ")
    var sent: [String] = []
    composer.onSend = { sent.append($0) }
    composer.sendTapped()
    XCTAssertEqual(sent, ["Hello crew"])
    XCTAssertEqual(composer.input.text, "")
    XCTAssertFalse(composer.send.isEnabled)
    composer.sendTapped()
    XCTAssertEqual(sent.count, 1)
  }

  func testAControlledValueOnlyChangesWhenTheApplicationAnswers() {
    let composer = ShadowListUtilsChatInput(defaultValue: "draft")
    composer.value = "Current"
    var changes: [String] = []
    var sent: [String] = []
    composer.onChangeText = { changes.append($0) }
    composer.onSend = { sent.append($0) }
    composer.input.text = "Requested"
    composer.textViewDidChange(composer.input)
    XCTAssertEqual(changes, ["Requested"])
    XCTAssertEqual(composer.input.text, "Current")
    composer.sendTapped()
    XCTAssertEqual(sent, ["Current"])
    XCTAssertEqual(composer.input.text, "Current")
    composer.onChangeText = { composer.value = $0 }
    composer.input.text = "Accepted"
    composer.textViewDidChange(composer.input)
    XCTAssertEqual(composer.value, "Accepted")
  }

  func testDisabledWhitespaceLimitsAndLabelsFollowTheContract() {
    let composer = ShadowListUtilsChatInput(defaultValue: " ")
    XCTAssertFalse(composer.send.isEnabled)
    composer.value = "Hello"
    composer.disabled = true
    var sent = false
    composer.onSend = { _ in sent = true }
    composer.sendTapped()
    XCTAssertFalse(sent)
    XCTAssertFalse(composer.input.isEditable)
    composer.disabled = false
    composer.maxLength = 6
    XCTAssertFalse(composer.textView(composer.input, shouldChangeTextIn: NSRange(location: 5, length: 0), replacementText: "😀"))
    XCTAssertTrue(composer.textView(composer.input, shouldChangeTextIn: NSRange(location: 0, length: 5), replacementText: "😀"))
    composer.labels.send = "Envoyer"
    XCTAssertEqual(composer.send.accessibilityLabel, "Envoyer")
  }
}
