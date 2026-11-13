import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class AssistantComponentsTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  func testScrollButtonRetainsItsSlotButHiddenStateCannotActOrTakeAccessibilityFocus() {
    let button = ShadowListUtilsAssistantScrollButton(theme: theme)
    var presses = 0
    button.onPress = { presses += 1 }
    button.performPress()
    XCTAssertEqual(presses, 0)
    XCTAssertFalse(button.isUserInteractionEnabled)
    XCTAssertTrue(button.accessibilityElementsHidden)
    button.visible = true
    button.performPress()
    XCTAssertEqual(presses, 1)
    XCTAssertTrue(button.isAccessibilityElement)
    XCTAssertEqual(button.intrinsicContentSize, CGSize(width: 36, height: 36))
    button.visible = false
    XCTAssertEqual(button.intrinsicContentSize, CGSize(width: 36, height: 36))
    button.onPress = nil
    button.visible = true
    XCTAssertFalse(button.isEnabled)
  }

  func testThinkingDisclosureUsesUTF16TailAndKeepsApplicationTextUnchanged() {
    let text = "Prefix " + String(repeating: "Reasoning 👋🏽 text. ", count: 50)
    let view = ShadowListUtilsAssistantThinking(thinking: text, active: true, theme: theme)
    let collapsed = view.sizeThatFits(CGSize(width: 300, height: CGFloat.greatestFiniteMagnitude))
    let layout = ShadowListUtilsAssistantThinking.layout(thinking: text, title: "Thinking", expanded: false, width: 300, theme: theme)
    XCTAssertEqual(layout.text, String(decoding: Array(text.utf16.suffix(160)), as: UTF16.self))
    view.toggle()
    XCTAssertGreaterThan(view.sizeThatFits(CGSize(width: 300, height: CGFloat.greatestFiniteMagnitude)).height, collapsed.height)
    XCTAssertEqual(view.thinking, text)
    XCTAssertEqual(ShadowListUtilsAssistantThinking.title(thinkingMs: 1499, active: false, labels: view.labels), "Thought for 1s")
    XCTAssertEqual(ShadowListUtilsAssistantThinking.title(thinkingMs: 1500, active: false, labels: view.labels), "Thought for 2s")
    XCTAssertEqual(ShadowListUtilsAssistantThinking.title(thinkingMs: .nan, active: true, labels: view.labels), "Thinking")
  }

  func testToolStatusOutputAndDisclosureShareTranscriptGeometryWithoutChangingCall() {
    let call = ShadowListUtilsAssistantToolCall(key: "search", name: "Search", input: String(repeating: "Input\n", count: 20), status: .done(output: String(repeating: "Output\n", count: 20)))
    let view = ShadowListUtilsAssistantToolCallCard(call: call, theme: theme)
    let before = view.sizeThatFits(CGSize(width: 300, height: CGFloat.greatestFiniteMagnitude))
    view.toggle()
    let after = view.sizeThatFits(CGSize(width: 300, height: CGFloat.greatestFiniteMagnitude))
    XCTAssertGreaterThan(after.height, before.height)
    XCTAssertEqual(view.call, call)
    let reply = ShadowListUtilsAssistantReply(key: "reply", variants: [ShadowListUtilsAssistantTurn(toolCalls: [call], status: .done())])
    let layout = ShadowListUtilsAssistantReplyMessage.layout(message: reply, live: nil, width: 332, busy: false, isLatest: false, thinkingExpanded: false, expandedToolKeys: [call.key], hasFeedback: false, hasRetry: false, theme: theme, labels: view.labels)
    let frame = layout.sections.compactMap { section -> CGRect? in if case let .tool(_, _, _, _, frame, _) = section { return frame }; return nil }.first
    XCTAssertEqual(frame?.height, after.height)
    view.call = ShadowListUtilsAssistantToolCall(key: "other", name: "Run", input: "", status: .stopped)
    XCTAssertFalse(view.expanded)
    view.grouped = true
    view.separated = true
    XCTAssertEqual(view.backgroundColor, .clear)
  }

  func testEmptySuggestionsAreControlledAndWrapAtNarrowWidths() {
    let view = ShadowListUtilsAssistantEmpty(theme: theme)
    let item = ShadowListUtilsAssistantSuggestion(title: "Plan a trip", prompt: "Three days in Lisbon")
    view.suggestions = [item, item]
    var selected: ShadowListUtilsAssistantSuggestion?
    view.onSelectSuggestion = { selected = $0 }
    view.select(item)
    XCTAssertEqual(selected, item)
    XCTAssertEqual(view.suggestions, [item, item])
    XCTAssertGreaterThan(view.sizeThatFits(CGSize(width: 210, height: CGFloat.greatestFiniteMagnitude)).height,
                         view.sizeThatFits(CGSize(width: 390, height: CGFloat.greatestFiniteMagnitude)).height)
    var labels = view.labels
    labels.emptyTitle = "Where next?"
    labels.emptySubtitle = "Choose a prompt"
    view.labels = labels
    view.frame = CGRect(x: 0, y: 0, width: 390, height: 500)
    view.layoutIfNeeded()
    XCTAssertTrue(view.subviews.contains { $0.accessibilityLabel == "Where next?" })
  }

  func testTypingAnimationStopsOnDetachAndHasOneLocalizedAccessibilityElement() {
    let view = ShadowListUtilsAssistantTypingIndicator(theme: theme)
    var labels = view.labels
    labels.typing = "Preparing answer"
    view.labels = labels
    XCTAssertEqual(view.accessibilityLabel, "Preparing answer")
    XCTAssertTrue(view.isAccessibilityElement)
    let window = UIWindow(frame: CGRect(x: 0, y: 0, width: 390, height: 800))
    window.addSubview(view)
    window.isHidden = false
    view.isHidden = true
    XCTAssertTrue(view.subviews.allSatisfy { $0.layer.animation(forKey: "pulse") == nil })
    view.removeFromSuperview()
    XCTAssertTrue(view.subviews.allSatisfy { $0.layer.animation(forKey: "pulse") == nil })
  }

  func testPollFooterLocalizesTotalAndListUsesStandaloneViewWithoutTakingCustomFooter() {
    let view = ShadowListUtilsPollFooter(totalVotes: 3, theme: theme)
    var labels = ShadowListUtilsPollLabels()
    labels.totalVotes = { "Votes: \(Int($0))" }
    view.labels = labels
    view.totalVotes = 7
    XCTAssertTrue(view.subviews.contains { child in child.subviews.contains { $0.accessibilityLabel == "Votes: 7" } })
    let list = ShadowListUtilsPollList(theme: theme)
    XCTAssertTrue(list.footerView is ShadowListUtilsPollFooter)
    let custom = UIView()
    list.footerView = custom
    list.poll = ShadowListUtilsPollData(question: "Where?", options: [])
    XCTAssertTrue(list.footerView === custom)
    list.footerView = nil
    list.labels = labels
    XCTAssertNil(list.footerView)
  }
  func testSelectableDisclosureNativePaintAndMeasuredLinesMatchAcrossWidthsAndUnicode() {
    for text in ["A selectable thought. \nNext line 👋🏽 https://example.com", String(repeating: "مرحبا بالعالم. ", count: 8), "Text ending in a line break\n"] {
      for width in [CGFloat(100), CGFloat(280)] {
        let layout = ShadowListUtilsAssistantDisclosureTextLayout(text: text, width: width, style: theme.typography.footnote, theme: theme, expanded: true)
        let view = UITextView(frame: CGRect(x: 0, y: 0, width: width, height: layout.size.height), textContainer: nil)
        layout.apply(to: view)
        view.layoutManager.ensureLayout(for: view.textContainer)
        let used = view.layoutManager.usedRect(for: view.textContainer)
        let height = max(used.maxY, view.layoutManager.extraLineFragmentRect.maxY)
        XCTAssertEqual(layout.size.height, ShadowListUtilsPlatform.ceilToPixel(height), "width=\(width), text=\(text.debugDescription), used=\(used), extra=\(view.layoutManager.extraLineFragmentRect), typing=\(view.typingAttributes)")
        XCTAssertTrue(view.isSelectable)
        XCTAssertFalse(view.isEditable)
        XCTAssertEqual(view.textContainerInset, .zero)
        XCTAssertEqual(view.textContainer.lineFragmentPadding, 0)
        XCTAssertEqual(view.attributedText.string, text)
        XCTAssertEqual(view.dataDetectorTypes, [])
        view.selectedRange = NSRange(location: 0, length: min(3, (text as NSString).length))
        XCTAssertEqual(view.selectedRange.length, min(3, (text as NSString).length))
      }
    }
  }

}
