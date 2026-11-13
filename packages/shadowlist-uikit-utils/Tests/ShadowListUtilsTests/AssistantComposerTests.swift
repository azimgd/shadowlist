@testable import ShadowListUtils
import XCTest

@MainActor
final class AssistantComposerTests: XCTestCase {
  func testAMissingSendHandlerKeepsTheDraftAndDisablesSending() {
    let composer = ShadowListUtilsAssistantComposer()
    composer.setDraft("Keep this draft", focus: false)
    XCTAssertFalse(composer.primary.isEnabled)
    composer.submit()
    XCTAssertEqual(composer.getDraft(), "Keep this draft")
  }

  func testJsWhitespaceDoesNotBecomeADraftOrLeakIntoTheSentText() {
    let composer = ShadowListUtilsAssistantComposer()
    composer.setDraft("\u{FEFF}\u{3000}", focus: false)
    XCTAssertFalse(composer.primary.isEnabled)
    var sent: String?
    composer.onSend = { text, _ in sent = text }
    composer.setDraft("\u{FEFF} Hello \u{FEFF}", focus: false)
    composer.submit()
    XCTAssertEqual(sent, "Hello")
  }

  func testAttachmentsAloneCanSendAndTheirRemovalRemainsApplicationOwned() {
    let composer = ShadowListUtilsAssistantComposer()
    let attachment = ShadowListUtilsAssistantAttachment(key: "ticket", kind: .file, name: "Ticket.pdf")
    composer.attachments = [attachment]
    var sent: String?
    var sentAttachments: [ShadowListUtilsAssistantAttachment] = []
    composer.onSend = { text, attachments in sent = text; sentAttachments = attachments }
    composer.submit()
    XCTAssertEqual(sent, "")
    XCTAssertEqual(sentAttachments, [attachment])
    XCTAssertEqual(composer.attachments, [attachment])
    XCTAssertEqual(composer.getDraft(), "")
  }

  func testControlledDraftCommandsAndSendingRequestChangesWithoutChangingTheValue() {
    let composer = ShadowListUtilsAssistantComposer()
    composer.value = "Current"
    var changes: [String] = []
    var sent: String?
    composer.onChangeText = { changes.append($0) }
    composer.onSend = { text, _ in sent = text }
    composer.setDraft("Requested", focus: false)
    XCTAssertEqual(changes, ["Requested"])
    XCTAssertEqual(composer.getDraft(), "Current")
    composer.submit()
    XCTAssertEqual(sent, "Current")
    XCTAssertEqual(changes, ["Requested", ""])
    XCTAssertEqual(composer.value, "Current")
    composer.onChangeText = { composer.value = $0 }
    composer.setDraft("Accepted", focus: false)
    XCTAssertEqual(composer.getDraft(), "Accepted")
  }

  func testStopAndSendWhileStreamingFollowTheActualDraftCondition() {
    let composer = ShadowListUtilsAssistantComposer()
    var stopped = 0
    var sent: [String] = []
    composer.onStop = { stopped += 1 }
    composer.onSend = { text, _ in sent.append(text) }
    composer.streaming = true
    composer.sendWhileStreaming = true
    composer.submit()
    XCTAssertEqual(stopped, 1)
    composer.setDraft("  Next question  ", focus: false)
    composer.submit()
    XCTAssertEqual(sent, ["Next question"])
    XCTAssertEqual(composer.getDraft(), "")
    composer.setDraft("Keep this draft", focus: false)
    composer.sendWhileStreaming = false
    composer.submit()
    XCTAssertEqual(stopped, 2)
    XCTAssertEqual(composer.getDraft(), "Keep this draft")
  }

  func testLabelsAndLengthUseTheAssistantDefaults() {
    let composer = ShadowListUtilsAssistantComposer()
    XCTAssertEqual(composer.maxLength, 4000)
    XCTAssertEqual(composer.input.maxLength, 4000)
    composer.labels.placeholder = "Ask your crew"
    composer.labels.send = "Send question"
    XCTAssertEqual(composer.input.input.accessibilityLabel, "Ask your crew")
    XCTAssertEqual(composer.primary.accessibilityLabel, "Send question")
  }
}
