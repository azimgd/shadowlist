import ShadowListKit
@testable import ShadowListUtils
import ShadowListUtilsCore
import UIKit
import XCTest

@MainActor
final class ChatTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light
  private let me = ShadowListUtilsChatAuthor(key: "me", name: "Me")
  private let ada = ShadowListUtilsChatAuthor(key: "ada", name: "Ada Lovelace")

  private func message(
    _ index: Int, own: Bool = false, text: String? = "Hello there", images: [String] = [], status: ShadowListUtilsChatMessageStatus? = nil
  ) -> ShadowListUtilsChatMessage {
    ShadowListUtilsChatMessage(key: "message-\(index)", author: own ? me : ada, isOwn: own, text: text, images: images, status: status)
  }

  private func layout(
    _ item: ShadowListUtilsChatMessage, caption: String? = nil, showsAuthor: Bool = true, width: CGFloat = testWidth
  ) throws -> ShadowListUtilsChatBubbleLayout {
    let bubble = ShadowListUtilsChatBubble(item: item, caption: caption, showsAuthor: showsAuthor, theme: theme, labels: ShadowListUtilsChatLabels())
    return try XCTUnwrap(bubble.layout(width: width) as? ShadowListUtilsChatBubbleLayout)
  }

  func testTheListShowsItsItems() {
    let list = ShadowListUtilsChatList(theme: theme)
    host(list)
    list.items = (0..<6).map { message($0, own: $0 % 2 == 0) }
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<6).map { "message-\($0)" })
    XCTAssertTrue(list.rowView(at: 5) is ShadowListUtilsChatBubbleView)
  }

  func testTheListIsInvertedAndOpensAtItsEnd() {
    let list = ShadowListUtilsChatList(theme: theme)
    XCTAssertTrue(list.isInverted)
    XCTAssertFalse(list.followAppends)
    XCTAssertFalse(list.allowsSelection)
    XCTAssertTrue(list.showsAuthor)
    host(list)
    list.items = (0..<60).map { message($0) }
    list.layoutIfNeeded()
    XCTAssertNotNil(list.rowView(at: 59))
    XCTAssertNil(list.rowView(at: 0))
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() {
    let list = ShadowListUtilsChatList(theme: theme)
    host(list)
    var items = (0..<4).map { message($0) }
    list.items = items
    list.layoutIfNeeded()
    let untouched = list.rowView(at: 3)
    items[2].status = .failed
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 2)?.row as? ShadowListUtilsChatBubble)?.item.status, .failed)
    XCTAssertTrue(list.rowView(at: 3) === untouched)
    items.remove(at: 0)
    list.setItems(items, animated: true)
    XCTAssertEqual(list.keys, ["message-1", "message-2", "message-3"])
  }

  func testTheClosuresRunWithTheirPayload() throws {
    let list = ShadowListUtilsChatList(theme: theme)
    host(list)
    list.items = [message(0), message(1, own: true, status: .failed)]
    list.layoutIfNeeded()
    var calls: [String] = []
    list.onPressItem = { calls.append("press \($0.key)") }
    list.onStartReached = { calls.append("start") }
    list.tap(0)
    list.delegate?.listViewDidReachStart?(list)
    XCTAssertEqual(calls, ["press message-0", "start"])

    let failed = try XCTUnwrap(list.rowView(at: 1))
    XCTAssertNil(failed.accessibilityCustomActions)
    list.onRetryMessage = { calls.append("retry \($0.key)") }
    let actions = try XCTUnwrap(failed.accessibilityCustomActions)
    XCTAssertEqual(actions.map(\.name), ["Sends the message again"])
    XCTAssertEqual(actions[0].actionHandler?(actions[0]), true)
    XCTAssertEqual(calls.last, "retry message-1")
    XCTAssertNil(list.rowView(at: 0)?.accessibilityCustomActions)
  }

  func testATapOnTheFailedLineRetriesAndDoesNotPressTheMessage() throws {
    let list = ShadowListUtilsChatList(theme: theme)
    host(list)
    list.items = [message(0, own: true, status: .failed)]
    list.layoutIfNeeded()
    var calls: [String] = []
    list.onPressItem = { calls.append("press \($0.key)") }
    let view = try XCTUnwrap(list.rowView(at: 0))
    let layout = try layout(message(0, own: true, status: .failed))
    // Without onRetryMessage the line belongs to the row, whose tap presses the message.
    let line = CGPoint(x: layout.failedFrame.minX - 6, y: layout.failedFrame.midY)
    let retry = try XCTUnwrap(view.hitTest(line, with: nil))
    XCTAssertFalse(retry === view)
    let tap = try XCTUnwrap(retry.gestureRecognizers?.first as? UITapGestureRecognizer)
    XCTAssertFalse(view.gestureRecognizerShouldBegin(tap))
    list.onRetryMessage = { calls.append("retry \($0.key)") }
    XCTAssertTrue(view.gestureRecognizerShouldBegin(tap))
    XCTAssertEqual(retry.frame, layout.failedFrame.insetBy(dx: -8, dy: -8))
    XCTAssertTrue(view.hitTest(CGPoint(x: layout.bubbleFrame.midX, y: layout.bubbleFrame.midY), with: nil) !== retry)
  }

  func testAHoldOnABubbleReportsALongPressAndSuppressesTheTap() throws {
    let list = ShadowListUtilsChatList(theme: theme)
    host(list)
    list.items = [message(0, own: true)]
    list.layoutIfNeeded()
    var calls: [String] = []
    list.onPressItem = { calls.append("press \($0.key)") }
    // The kit adds its long press, which its tap waits for, only for a delegate that answers this.
    XCTAssertEqual(list.delegate?.responds(to: #selector(ShadowListKitListViewDelegate.listView(_:didLongPressItemAt:))), true)
    list.delegate?.listView?(list, didLongPressItemAt: 0)
    XCTAssertEqual(calls, [])
    list.onLongPressMessage = { calls.append("hold \($0.key)") }
    let cell = try XCTUnwrap(list.rowView(at: 0))
    let layout = try layout(message(0, own: true))
    _ = list.hitTest(cell.convert(CGPoint(x: layout.bubbleFrame.midX, y: layout.bubbleFrame.midY), to: list), with: nil)
    list.delegate?.listView?(list, didLongPressItemAt: 0)
    XCTAssertEqual(calls, ["hold message-0"])
    // A hold beside the bubble is no hold on the message.
    _ = list.hitTest(cell.convert(CGPoint(x: 4, y: layout.bubbleFrame.midY), to: list), with: nil)
    list.delegate?.listView?(list, didLongPressItemAt: 0)
    XCTAssertEqual(calls, ["hold message-0"])
  }

  func testCaptionForItemAndShowsAuthorReachTheRows() {
    let list = ShadowListUtilsChatList(theme: theme)
    host(list)
    list.items = [message(0), message(1)]
    list.layoutIfNeeded()
    let height = list.rectForItem(at: 0).height
    list.captionForItem = { $0.key == "message-0" ? "09:41" : nil }
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 0)?.row as? ShadowListUtilsChatBubble)?.caption, "09:41")
    XCTAssertEqual(list.rectForItem(at: 0).height, height + 4 + theme.typography.caption.lineHeight)
    list.showsAuthor = false
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 1)?.row as? ShadowListUtilsChatBubble)?.showsAuthor, false)
    XCTAssertEqual(list.rectForItem(at: 1).height, height - theme.typography.caption.lineHeight - 2)
  }

  func testATextMessageIsAsTallAsTheCorePredicts() throws {
    let metrics = ShadowListUtilsChatMetrics(
      spacingXxs: 2, spacingXs: 4, spacingSm: 8, spacingMd: 12, spacingLg: 16, bodyFontSize: 17, bodyLineHeight: 22,
      bodyLetterSpacing: -0.43, captionLineHeight: 16)
    for (own, showsAuthor, caption, status) in [
      (false, true, nil, nil), (false, false, nil, nil), (true, true, "09:41", nil), (false, true, "09:41", ShadowListUtilsChatMessageStatus.failed),
    ] as [(Bool, Bool, String?, ShadowListUtilsChatMessageStatus?)] {
      let item = message(0, own: own, text: String(repeating: "word ", count: 30), status: status)
      let layout = try layout(item, caption: caption, showsAuthor: showsAuthor)
      var spec = ShadowListUtilsChatSizeSpec()
      let shape = ShadowListUtilsChatShape(
        hasText: true, hasImages: false, isOwn: ObjCBool(own), showsAuthor: ObjCBool(showsAuthor), hasCaption: ObjCBool(caption != nil),
        failed: ObjCBool(status == .failed))
      XCTAssertTrue(ShadowListUtilsCore.chatMessageSizeSpec(&spec, shape: shape, metrics: metrics))
      let text = try XCTUnwrap(layout.text)
      assertLines(text, theme.typography.body)
      XCTAssertEqual(layout.height, text.size.height + spec.insetHeight)
      XCTAssertEqual(layout.maximumBubbleWidth, spec.widthFraction * (testWidth - 24), accuracy: 1e-9)
      XCTAssertLessThanOrEqual(layout.bubbleFrame.width, layout.maximumBubbleWidth)
      // The frames end where the predicted height ends.
      let bottom = max(layout.bubbleFrame.maxY, layout.failed == nil ? 0 : layout.failedFrame.minY + theme.typography.caption.lineHeight)
      XCTAssertEqual(bottom + 2, layout.height)
    }
  }

  func testTheBubbleLayoutHoldsItsMetrics() throws {
    let caption = theme.typography.caption
    let other = try layout(message(0))
    XCTAssertEqual(other.maximumBubbleWidth, 0.75 * (testWidth - 24))
    XCTAssertEqual(other.nameFrame.origin, CGPoint(x: 12 + 30 + 8 + 12, y: 2))
    XCTAssertEqual(other.bubbleFrame.origin, CGPoint(x: 12 + 30 + 8, y: 2 + caption.lineHeight + 2))
    XCTAssertEqual(other.textFrame.origin, CGPoint(x: other.bubbleFrame.minX + 14, y: other.bubbleFrame.minY + 8))
    XCTAssertEqual(other.bubbleFrame.width, other.textFrame.width + 28)
    XCTAssertEqual(other.bubbleFrame.height, theme.typography.body.lineHeight + 16)
    XCTAssertEqual(other.avatarFrame, CGRect(x: 12, y: other.height - 2 - 2 - 30, width: 30, height: 30))

    let own = try layout(message(1, own: true))
    XCTAssertNil(own.name)
    XCTAssertEqual(own.bubbleFrame.maxX, testWidth - 12)
    XCTAssertEqual(own.bubbleFrame.minY, 2)
    XCTAssertEqual(own.avatarFrame, .zero)
    XCTAssertEqual(own.height, 2 + theme.typography.body.lineHeight + 16 + 2)

    let hidden = try layout(message(2), showsAuthor: false)
    XCTAssertNil(hidden.name)
    XCTAssertEqual(hidden.bubbleFrame.origin, CGPoint(x: 12 + 30 + 8, y: 2))

    let captioned = try layout(message(3, own: true), caption: "09:41")
    XCTAssertEqual(captioned.captionFrame.origin, CGPoint(x: captioned.textFrame.minX, y: captioned.textFrame.maxY + 4))
    XCTAssertEqual(captioned.bubbleFrame.height, theme.typography.body.lineHeight + 4 + caption.lineHeight + 16)

    let failed = try layout(message(4, own: true, status: .failed))
    XCTAssertEqual(failed.failedFrame.minY, failed.bubbleFrame.maxY + 4)
    XCTAssertEqual(failed.failedFrame.maxX, testWidth - 12)
  }

  func testAnEmptyCaptionAddsNoLine() throws {
    let plain = try layout(message(0, own: true))
    let empty = try layout(message(0, own: true), caption: "")
    XCTAssertNil(empty.caption)
    XCTAssertEqual(empty.captionFrame, .zero)
    XCTAssertEqual(empty.height, plain.height)
    XCTAssertEqual(empty.bubbleFrame, plain.bubbleFrame)
    let images = try layout(message(1, text: nil, images: ["a"]), caption: "")
    XCTAssertNil(images.caption)
    XCTAssertEqual(images.height, 2 + 2 + 320 + 2 + 2)
  }

  func testAMessageWithoutTextOrImagesKeepsItsAvatarInsideTheRow() throws {
    let empty = try layout(message(0, text: nil))
    XCTAssertEqual(empty.height, 30 + 3 * 2)
    XCTAssertEqual(empty.avatarFrame, CGRect(x: 12, y: 2, width: 30, height: 30))
    XCTAssertEqual(try layout(message(1, text: "")).avatarFrame.minY, 2)
    XCTAssertEqual(try layout(message(2, own: true, text: nil)).height, 2 + 2)
  }

  func testImagesShowLargeOrAsAGridOfFour() throws {
    let single = try layout(message(0, text: nil, images: ["a"]))
    XCTAssertEqual(single.imagesFrame, CGRect(x: 12 + 30 + 8, y: 4, width: 240, height: 320))
    XCTAssertEqual(single.imageFrames, [CGRect(x: 0, y: 0, width: 240, height: 320)])
    XCTAssertEqual(single.height, 2 + 2 + 320 + 2 + 2)
    XCTAssertNil(single.name)
    XCTAssertNil(single.text)

    let grid = try layout(message(1, own: true, text: nil, images: ["a", "b", "c", "d", "e"]), caption: "09:41")
    XCTAssertEqual(grid.imagesFrame, CGRect(x: testWidth - 12 - 240, y: 4, width: 240, height: 240))
    XCTAssertEqual(
      grid.imageFrames,
      [
        CGRect(x: 0, y: 0, width: 119, height: 119), CGRect(x: 121, y: 0, width: 119, height: 119),
        CGRect(x: 0, y: 121, width: 119, height: 119), CGRect(x: 121, y: 121, width: 119, height: 119),
      ])
    XCTAssertEqual(grid.captionFrame.minY, grid.imagesFrame.maxY + 2 + 4)
    XCTAssertEqual(grid.captionFrame.maxX, testWidth - 12)
    XCTAssertEqual(grid.height, 2 + 2 + 240 + 2 + 4 + theme.typography.caption.lineHeight + 2 + 2)
  }

  func testACaptionUnderImagesWrapsAtTheTextMessageWidth() throws {
    let long = String(repeating: "A caption that goes on. ", count: 8)
    let captioned = try layout(message(0, text: nil, images: ["a"]), caption: long)
    let limit = SHADOWLIST_UTILS_CHAT_BUBBLE_WIDTH_FRACTION * testWidth - 46
    XCTAssertEqual(captioned.caption?.lineCount, 1)
    XCTAssertEqual(captioned.captionFrame.width, limit, accuracy: 1)
    XCTAssertGreaterThan(captioned.captionFrame.width, 240)
    XCTAssertEqual(captioned.captionFrame.minX, 12 + 30 + 8)
    XCTAssertEqual(captioned.maximumBubbleWidth, limit + 2 * SHADOWLIST_UTILS_CHAT_BUBBLE_PADDING_HORIZONTAL, accuracy: 1e-9)
  }

  func testARightToLeftBubbleMirrorsItsFrames() throws {
    let own = message(0, own: true, text: nil, images: ["a", "b"], status: .failed)
    let other = message(1)
    let (plainOwn, plainOther) = (try layout(own), try layout(other))
    try withRightToLeft {
      let mirrored = try layout(own)
      XCTAssertEqual(mirrored.imagesFrame.minX, 12)
      XCTAssertEqual(mirrored.imageFrames.first, CGRect(x: 121, y: 0, width: 119, height: 119))
      XCTAssertEqual(mirrored.failedFrame.minX, testWidth - plainOwn.failedFrame.maxX)
      let mirroredOther = try layout(other)
      XCTAssertEqual(mirroredOther.avatarFrame.maxX, testWidth - 12)
      XCTAssertEqual(mirroredOther.bubbleFrame.maxX, testWidth - plainOther.bubbleFrame.minX)
      XCTAssertEqual(mirroredOther.textFrame.minX, testWidth - plainOther.textFrame.maxX)
      XCTAssertEqual(mirroredOther.height, plainOther.height)
    }
  }

  func testTheBubbleReadsItsAuthorTextAndState() {
    let view = ShadowListUtilsChatBubbleView(reuseIdentifier: nil)
    let labels = ShadowListUtilsChatLabels()
    func show(_ item: ShadowListUtilsChatMessage, caption: String? = nil) {
      view.configure(ShadowListUtilsChatBubble(item: item, caption: caption, showsAuthor: true, theme: theme, labels: labels), layout: nil)
    }
    show(message(0), caption: "09:41")
    XCTAssertEqual(view.accessibilityLabel, "Ada Lovelace, Hello there, 09:41")
    show(message(0), caption: "")
    XCTAssertEqual(view.accessibilityLabel, "Ada Lovelace, Hello there")
    XCTAssertNil(view.accessibilityHint)
    show(message(1, own: true, status: .sending))
    XCTAssertEqual(view.accessibilityLabel, "Hello there")
    XCTAssertEqual(view.accessibilityHint, "Sending")
    show(message(2, own: true, text: nil, images: ["a", "b"], status: .failed))
    XCTAssertEqual(view.accessibilityLabel, "Image, Image, Not delivered. Tap to retry.")
  }
}
