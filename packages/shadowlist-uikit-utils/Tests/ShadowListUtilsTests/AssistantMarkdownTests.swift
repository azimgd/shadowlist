@testable import ShadowListUtils
import ShadowListUtilsCore
import UIKit
import XCTest

@MainActor
final class AssistantMarkdownTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  func testStableBlocksKeepTheirViewsWhileTheLastBlockAndStreamingDotGrow() throws {
    let markdown = ShadowListUtilsAssistantMarkdown(text: "# Title\n\nA reply", theme: theme)
    let first = try XCTUnwrap(markdown.subviews.compactMap { $0 as? ShadowListUtilsAssistantMarkdownBlock }.first)
    let height = markdown.sizeThatFits(CGSize(width: 250, height: 1000)).height
    markdown.text += " that keeps growing over several lines with a useful answer for the crew."
    XCTAssertTrue(markdown.subviews.contains { $0 === first })
    XCTAssertGreaterThan(markdown.sizeThatFits(CGSize(width: 250, height: 1000)).height, height)
    let before = markdown.sizeThatFits(CGSize(width: 250, height: 1000)).height
    markdown.streaming = true
    XCTAssertGreaterThan(markdown.sizeThatFits(CGSize(width: 250, height: 1000)).height, before)
  }

  func testDefaultUnsafeLinksAreBlockedAndACustomHandlerGetsTheOriginalTarget() throws {
    let block = try XCTUnwrap(ShadowListUtilsCore.parseMarkdown("[Call](tel:+123)").first)
    let view = ShadowListUtilsAssistantMarkdownBlock(block: block, theme: theme, labels: ShadowListUtilsAssistantLabels())
    XCTAssertFalse(view.open("tel:+123"))
    var target: String?
    view.onOpenLink = { target = $0 }
    XCTAssertTrue(view.open("  tel:+123  "))
    XCTAssertEqual(target, "  tel:+123  ")
  }

  func testCodeWaitsForTheClosingFenceAndCopiedStateResetsAfterItsDelayAndDetach() throws {
    let open = try XCTUnwrap(ShadowListUtilsCore.parseMarkdown("```swift\nlet text = 1").first)
    let closed = try XCTUnwrap(ShadowListUtilsCore.parseMarkdown("```swift\nlet text = 1\n```").first)
    let pending = ShadowListUtilsAssistantMarkdownBlock(block: open, theme: theme, labels: ShadowListUtilsAssistantLabels())
    XCTAssertTrue(try XCTUnwrap(pending.subviews.compactMap { $0 as? UIButton }.first).isHidden)
    let ready = ShadowListUtilsAssistantMarkdownBlock(block: closed, theme: theme, labels: ShadowListUtilsAssistantLabels())
    let button = try XCTUnwrap(ready.subviews.compactMap { $0 as? UIButton }.first)
    var copied: String?
    ready.onCopyCode = { copied = $0 }
    ready.copyCode()
    XCTAssertEqual(copied, "let text = 1")
    XCTAssertEqual(button.accessibilityLabel, "Copied")
    let reset = expectation(description: "copied reset")
    DispatchQueue.main.asyncAfter(deadline: .now() + 1.6) {
      XCTAssertEqual(button.accessibilityLabel, "Copy code")
      reset.fulfill()
    }
    wait(for: [reset], timeout: 2)
    ready.copyCode()
    ready.didMoveToWindow()
    XCTAssertEqual(button.accessibilityLabel, "Copy code")
  }

  func testTheParserBridgePreservesEmbeddedNullCharacters() throws {
    let text = "before\0after"
    let block = try XCTUnwrap(ShadowListUtilsCore.parseMarkdown(text).first)
    XCTAssertEqual(block.raw, text)
    XCTAssertEqual(block.runs.first?.first?.text, text)
  }
}
