import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class AssistantListTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light
  private var prompt: ShadowListUtilsAssistantPrompt { ShadowListUtilsAssistantPrompt(key: "prompt", text: "Plan a trip") }
  private var reply: ShadowListUtilsAssistantReply {
    ShadowListUtilsAssistantReply(key: "reply", variants: [
      ShadowListUtilsAssistantTurn(content: "Saved **answer**", status: .done(followUps: ["Next question"])),
      ShadowListUtilsAssistantTurn(content: "Another answer", status: .done()),
    ])
  }

  func testHiddenThinkingAndPagerDoNotCallApplicationFormatters() {
    let list = ShadowListUtilsAssistantList(theme: theme)
    var labels = ShadowListUtilsAssistantLabels()
    labels.thoughtFor = { _ in XCTFail("Thinking is absent"); return "" }
    labels.versionPosition = { _, _ in XCTFail("The pager is absent"); return "" }
    list.labels = labels
    list.items = [.assistant(ShadowListUtilsAssistantReply(key: "empty")), .endMarker]
    XCTAssertGreaterThan(list.rows[0].layout(width: testWidth).height, 1)
  }

  func testTheTranscriptOwnsNoMessagesOrTurnsAndEndRowsStayRealButDoNotAnchor() {
    let list = ShadowListUtilsAssistantList(theme: theme)
    list.nonAnchorKeys = ["custom"]
    let items: [ShadowListUtilsAssistantMessage] = [.user(prompt), .assistant(reply), .endMarker]
    list.items = items
    XCTAssertEqual(list.keys, ["prompt", "reply", "assistant-end"])
    XCTAssertTrue(list.isInverted)
    XCTAssertTrue(list.followAppends)
    XCTAssertFalse(list.allowsSelection)
    XCTAssertEqual(list.nonAnchorKeys, ["custom", "assistant-end"])
    XCTAssertEqual(list.rows[2].layout(width: testWidth).height, 1)
    let previous = list.rows[1].contentVersion
    let live = ShadowListUtilsAssistantTurn(content: "A growing live answer")
    list.turns = ["reply": live]
    XCTAssertNotEqual(previous, list.rows[1].contentVersion)
    XCTAssertEqual(list.items, items)
    XCTAssertEqual(ShadowListUtilsAssistantReplyMessage.resolvedTurn(message: reply, live: live), live)
    list.turns = [:]
    XCTAssertEqual(ShadowListUtilsAssistantReplyMessage.resolvedTurn(message: reply, live: nil), reply.variants[0])
    list.items = [.user(prompt)]
    XCTAssertEqual(list.nonAnchorKeys, ["custom"])
  }

  func testAnInvalidSavedVariantFallsBackToAnEmptyDoneTurn() {
    var value = reply
    value.variantIndex = 100
    XCTAssertEqual(ShadowListUtilsAssistantReplyMessage.resolvedTurn(message: value, live: nil), ShadowListUtilsAssistantTurn(status: .done()))
    value.variantIndex = -1
    XCTAssertEqual(ShadowListUtilsAssistantReplyMessage.resolvedTurn(message: value, live: nil).content, "")
  }

  func testCallbacksRequestFeedbackVariantsAndRegenerationWithoutMutatingTheModel() {
    let view = ShadowListUtilsAssistantReplyMessage(message: reply, theme: theme)
    var feedback: ShadowListUtilsAssistantFeedback?
    var selected: Int?
    var regenerated = 0
    view.onFeedback = { key, value in XCTAssertEqual(key, "reply"); feedback = value }
    view.onSelectVariant = { key, value in XCTAssertEqual(key, "reply"); selected = value }
    view.onRegenerate = { _ in regenerated += 1 }
    view.perform(.good)
    view.perform(.next)
    view.perform(.regenerate)
    XCTAssertEqual(feedback, .good)
    XCTAssertEqual(selected, 1)
    XCTAssertEqual(regenerated, 1)
    XCTAssertEqual(view.message, reply)
    view.busy = true
    view.perform(.regenerate)
    XCTAssertEqual(regenerated, 1)
    view.turn = ShadowListUtilsAssistantTurn(content: "streaming")
    view.perform(.bad)
    XCTAssertEqual(feedback, .good)
  }

  func testLocalUserDisclosureChangesTheImmutableRowLayoutWithoutChangingMessages() throws {
    let list = ShadowListUtilsAssistantList(theme: theme)
    let items: [ShadowListUtilsAssistantMessage] = [.user(prompt), .assistant(reply), .endMarker]
    list.items = items
    let window = host(list)
    let previous = list.rows[0]
    let previousHeight = previous.layout(width: testWidth).height
    let cell = try XCTUnwrap(list.cellForItem(at: 0))
    let user = try XCTUnwrap(cell.subviews.compactMap { $0 as? ShadowListUtilsAssistantUserMessage }.first)
    user.toggleActions()
    XCTAssertEqual(list.items, items)
    XCTAssertNotEqual(list.rows[0].contentVersion, previous.contentVersion)
    XCTAssertGreaterThan(list.rows[0].layout(width: testWidth).height, previousHeight)
    XCTAssertEqual(previous.layout(width: testWidth).height, previousHeight)
    withExtendedLifetime(window) {}
  }

  func testStepsInterleaveToolsAndLatestFollowUpsOnlyAppearAtTheCurrentEnd() {
    let turn = ShadowListUtilsAssistantTurn(content: "ABCD", toolCalls: [
      ShadowListUtilsAssistantToolCall(key: "second", name: "Second", input: "B", at: 3, status: .done(output: "Done")),
      ShadowListUtilsAssistantToolCall(key: "first", name: "First", input: "A", at: 1, status: .stopped),
    ], status: .done(followUps: ["Next"]))
    let message = ShadowListUtilsAssistantReply(key: "reply", variants: [turn])
    func layout(latest: Bool) -> ShadowListUtilsAssistantReplyMessage.Layout {
      ShadowListUtilsAssistantReplyMessage.layout(message: message, live: nil, width: testWidth, busy: false,
        isLatest: latest, thinkingExpanded: false, expandedToolKeys: [], hasFeedback: false, hasRetry: false,
        theme: theme, labels: ShadowListUtilsAssistantLabels())
    }
    var order: [String] = []
    for section in layout(latest: true).sections {
      switch section {
      case let .markdown(_, text, _, _): order.append(text)
      case let .tool(call, _, _, _, _, _): order.append(call.key)
      default: break
      }
    }
    XCTAssertEqual(order, ["A", "first", "BC", "second", "D"])
    XCTAssertGreaterThan(layout(latest: true).size.height, layout(latest: false).size.height)
  }

  func testBackgroundRowTypesettingDoesNotCallApplicationLabelFormatters() {
    let list = ShadowListUtilsAssistantList(theme: theme)
    var labels = ShadowListUtilsAssistantLabels()
    labels.thoughtFor = { _ in XCTAssertTrue(Thread.isMainThread); return "Thought" }
    labels.toolStatus = { _ in XCTAssertTrue(Thread.isMainThread); return "Tool done" }
    labels.versionPosition = { _, _ in XCTAssertTrue(Thread.isMainThread); return "Version" }
    list.labels = labels
    let turn = ShadowListUtilsAssistantTurn(content: "A reply", thinking: "A reason", thinkingMs: 1000,
      toolCalls: [ShadowListUtilsAssistantToolCall(key: "tool", name: "Tool", input: "Input", status: .done(output: "Output"))], status: .done())
    list.items = [.assistant(ShadowListUtilsAssistantReply(key: "reply", variants: [turn])), .endMarker]
    let row = list.rows[0]
    let finished = expectation(description: "background typesetting")
    DispatchQueue.global().async {
      XCTAssertGreaterThan(row.layout(width: 390).height, 1)
      finished.fulfill()
    }
    wait(for: [finished], timeout: 2)
  }
}
