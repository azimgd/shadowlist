import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class FeedTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  private func item(_ index: Int, text: String? = "A short post.", images: [ShadowListUtilsFeedImage] = []) -> ShadowListUtilsFeedItem {
    ShadowListUtilsFeedItem(
      key: "post-\(index)", author: ShadowListUtilsFeedAuthor(name: "Ada Lovelace", handle: "@ada"), text: text, images: images,
      createdAtMs: Date().timeIntervalSince1970 * 1000 - 3 * 3_600_000)
  }

  private func row(_ item: ShadowListUtilsFeedItem) -> ShadowListUtilsFeedRow {
    ShadowListUtilsFeedRow(item: item, theme: theme, labels: ShadowListUtilsFeedLabels())
  }

  func testTheListShowsItsItems() {
    let list = ShadowListUtilsFeedList(theme: theme)
    host(list)
    list.items = (0..<5).map { item($0) }
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<5).map { "post-\($0)" })
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsFeedRowView)
    XCTAssertEqual(list.numberOfColumns, 1)
    XCTAssertFalse(list.allowsSelection)
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() {
    let list = ShadowListUtilsFeedList(theme: theme)
    host(list)
    var items = (0..<4).map { item($0) }
    list.items = items
    list.layoutIfNeeded()
    let untouched = list.rowView(at: 0)
    let height = list.rectForItem(at: 1).height
    items[1].text = String(repeating: "A much longer post. ", count: 12)
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 1)?.row as? ShadowListUtilsFeedRow)?.item.text, items[1].text)
    XCTAssertGreaterThan(list.rectForItem(at: 1).height, height)
    XCTAssertTrue(list.rowView(at: 0) === untouched)
    items.remove(at: 0)
    list.setItems(items, animated: true)
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, ["post-1", "post-2", "post-3"])
  }

  func testTheClosuresRunWithTheirPayload() {
    let list = ShadowListUtilsFeedList(theme: theme)
    host(list)
    list.items = (0..<3).map { item($0) }
    var pressed: [String] = []
    var ends = 0
    list.onPressItem = { pressed.append($0.key) }
    list.onEndReached = { ends += 1 }
    list.tap(1)
    list.delegate?.listViewDidReachEnd?(list)
    XCTAssertEqual(pressed, ["post-1"])
    XCTAssertEqual(ends, 1)
  }

  func testEachImageIsAnAccessibilityElementAndAButtonWithOnPressImage() throws {
    let list = ShadowListUtilsFeedList(theme: theme)
    host(list)
    let images = [ShadowListUtilsFeedImage(url: "https://example.com/a.jpg", alt: "Harbor"), ShadowListUtilsFeedImage(url: "https://example.com/b.jpg")]
    list.items = [item(0, images: images)]
    list.layoutIfNeeded()
    let view = try XCTUnwrap(list.rowView(at: 0))
    XCTAssertFalse(view.isAccessibilityElement)
    var elements = try XCTUnwrap(view.accessibilityElements as? [NSObject])
    XCTAssertEqual(elements.map(\.accessibilityLabel), ["Ada Lovelace, @ada, 3h, A short post.", "Harbor", "Image"])
    XCTAssertEqual(elements.map(\.accessibilityTraits), [[], .image, .image])
    list.onPressImage = { _, _ in }
    list.onPressItem = { _ in }
    elements = try XCTUnwrap(view.accessibilityElements as? [NSObject])
    XCTAssertEqual(elements.map(\.accessibilityTraits), [.button, [.image, .button], [.image, .button]])
    // The text element ends where the images start, which keeps its activation off them.
    let strip = try XCTUnwrap(view.subviews.compactMap { $0 as? UIScrollView }.first)
    XCTAssertEqual((elements[0] as? UIAccessibilityElement)?.accessibilityFrameInContainerSpace.maxY, strip.frame.minY)
  }

  func testFormatTimeWritesTheAgeInPlaceOfTheLabels() {
    XCTAssertEqual(row(item(0)).time, "3h")
    let formatted = ShadowListUtilsFeedRow(item: item(0), theme: theme, labels: ShadowListUtilsFeedLabels(), formatTime: { _ in "earlier" })
    XCTAssertEqual(formatted.time, "earlier")
    var labels = ShadowListUtilsFeedLabels()
    labels.time.hours = { "\($0) hours" }
    XCTAssertEqual(ShadowListUtilsFeedRow(item: item(0), theme: theme, labels: labels).time, "3 hours")
    var dateless = item(0)
    dateless.createdAtMs = nil
    XCTAssertNil(row(dateless).time)
  }

  func testAnAppendKeepsTheRowsOfTheItemsBefore() {
    let list = ShadowListUtilsFeedList(theme: theme)
    host(list)
    var made: [String] = []
    list.rowForItem = { [theme] item in
      made.append(item.key)
      return ShadowListUtilsFeedRow(item: item, theme: theme, labels: ShadowListUtilsFeedLabels())
    }
    var items = (0..<3).map { item($0) }
    list.items = items
    list.layoutIfNeeded()
    let first = list.rowView(at: 0)
    made = []
    items.append(item(3))
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual(made, ["post-3"])
    XCTAssertEqual(list.keys, ["post-0", "post-1", "post-2", "post-3"])
    XCTAssertTrue(list.rowView(at: 0) === first)
    XCTAssertTrue(list.rowView(at: 3) is ShadowListUtilsFeedRowView)
    made = []
    items[1].text = "Edited."
    items.insert(item(9), at: 0)
    list.setItems(items, animated: false)
    XCTAssertEqual(made, ["post-9", "post-1"])
  }

  func testTheRowLayoutHoldsItsMetrics() throws {
    let subhead = theme.typography.subhead
    let layout = try XCTUnwrap(row(item(0)).layout(width: testWidth) as? ShadowListUtilsFeedRowLayout)
    XCTAssertEqual(layout.avatarFrame, CGRect(x: 16, y: 12, width: 40, height: 40))
    XCTAssertEqual(layout.nameFrame.origin, CGPoint(x: 68, y: 12))
    XCTAssertEqual(layout.handleFrame.minX, layout.nameFrame.maxX + 4)
    XCTAssertEqual(layout.timeFrame.minX, layout.handleFrame.maxX + 4)
    XCTAssertEqual(layout.textFrame.origin, CGPoint(x: 68, y: 12 + subhead.scaledLineHeight() + 2))
    XCTAssertEqual(layout.textFrame.width, testWidth - 68 - 16)
    assertLines(layout.text, subhead)
    XCTAssertEqual(layout.height, layout.textFrame.maxY + 12)
    XCTAssertEqual(
      layout.separatorFrame, CGRect(x: 68, y: layout.height - ShadowListUtilsTheme.hairline, width: testWidth - 68, height: ShadowListUtilsTheme.hairline))

    let long = try XCTUnwrap(row(item(1, text: String(repeating: "word ", count: 80))).layout(width: testWidth) as? ShadowListUtilsFeedRowLayout)
    XCTAssertEqual(long.height, long.textFrame.maxY + 12)
  }

  func testTheHeaderLineWrapsAtALargeTextSize() throws {
    var post = item(0)
    post.author.name = "A name long enough to take the line at a large text size"
    let subhead = theme.typography.subhead
    let plain = try XCTUnwrap(row(post).layout(width: testWidth) as? ShadowListUtilsFeedRowLayout)
    XCTAssertEqual(plain.name.lineCount, 1)
    XCTAssertEqual(plain.timeFrame.minY, plain.nameFrame.minY)
    XCTAssertEqual(plain.textFrame.minY, 12 + subhead.scaledLineHeight() + 2)
    let large = try XCTUnwrap(withTextSize(1.5) { row(post).layout(width: testWidth) } as? ShadowListUtilsFeedRowLayout)
    XCTAssertGreaterThan(large.name.lineCount, 1)
    XCTAssertEqual(large.nameFrame.origin, CGPoint(x: 68, y: 12))
    XCTAssertEqual(large.handleFrame.origin, CGPoint(x: 68, y: large.nameFrame.maxY + 4))
    XCTAssertEqual(large.timeFrame.minY, large.handleFrame.minY)
    XCTAssertEqual(large.timeFrame.minX, large.handleFrame.maxX + 4)
    XCTAssertLessThanOrEqual(large.nameFrame.maxX, testWidth - 16)
    XCTAssertEqual(large.textFrame.minY, large.handleFrame.maxY + 2)
  }

  func testAPostWithoutTextOrImagesIsAsTallAsItsAvatar() throws {
    var bare = item(2, text: nil)
    bare.author.handle = nil
    let layout = try XCTUnwrap(row(bare).layout(width: testWidth) as? ShadowListUtilsFeedRowLayout)
    XCTAssertEqual(layout.height, 12 + 40 + 12)
    XCTAssertEqual(layout.avatarFrame.maxY, layout.height - 12)
    XCTAssertEqual(row(item(3, text: "")).layout(width: testWidth).height, 12 + 40 + 12)
  }

  func testASingleImageKeepsItsAspectBetweenItsBounds() throws {
    let width = testWidth - 68 - 16
    func imageFrame(_ image: ShadowListUtilsFeedImage) throws -> CGRect {
      try XCTUnwrap(row(item(0, images: [image])).layout(width: testWidth) as? ShadowListUtilsFeedRowLayout).imageFrame
    }
    let top = 12 + theme.typography.subhead.scaledLineHeight() + 2 + theme.typography.subhead.scaledLineHeight() + 12
    XCTAssertEqual(try imageFrame(ShadowListUtilsFeedImage(url: "a", width: 1000, height: 1000)), CGRect(x: 68, y: top, width: width, height: width))
    XCTAssertEqual(try imageFrame(ShadowListUtilsFeedImage(url: "a", width: 600, height: 1600)).height, ShadowListUtilsPlatform.roundToPixel(width / 0.75))
    XCTAssertEqual(try imageFrame(ShadowListUtilsFeedImage(url: "a", width: 3000, height: 500)).height, ShadowListUtilsPlatform.roundToPixel(width * 9 / 16))
    XCTAssertEqual(try imageFrame(ShadowListUtilsFeedImage(url: "a")).height, 200)
  }

  func testSeveralImagesShowInAStripThatBleedsPastTheContent() throws {
    let images = (0..<3).map { ShadowListUtilsFeedImage(url: "https://example.com/\($0).jpg") }
    let layout = try XCTUnwrap(row(item(0, images: images)).layout(width: testWidth) as? ShadowListUtilsFeedRowLayout)
    XCTAssertEqual(layout.stripFrame.minX, 68 - 4)
    XCTAssertEqual(layout.stripFrame.size, CGSize(width: testWidth - 68 - 16 + 8, height: 200))
    XCTAssertEqual(layout.imageFrame, .zero)
    XCTAssertEqual(layout.height, layout.stripFrame.maxY + 12)
    XCTAssertEqual(layout.stripImageFrames, [0, 1, 2].map { CGRect(x: 4 + CGFloat($0) * 288, y: 0, width: 280, height: 200) })
    let view = ShadowListUtilsFeedRowView(reuseIdentifier: nil)
    view.frame = CGRect(x: 0, y: 0, width: testWidth, height: layout.height)
    view.configure(row(item(0, images: images)), layout: layout)
    view.layoutIfNeeded()
    let strip = try XCTUnwrap(view.subviews.compactMap { $0 as? UIScrollView }.first)
    XCTAssertTrue(strip.clipsToBounds)
    XCTAssertEqual(strip.frame, layout.stripFrame)
    XCTAssertEqual(strip.subviews.compactMap { $0 as? ShadowListUtilsImageView }.map(\.frame), layout.stripImageFrames)
    XCTAssertEqual(strip.contentSize, CGSize(width: 3 * 288, height: 200))
    XCTAssertEqual(strip.contentOffset.x, -4)
  }

  func testAStripKeepsItsScrollPositionUntilItsKeyChanges() throws {
    let images = (0..<4).map { ShadowListUtilsFeedImage(url: "https://example.com/\($0).jpg") }
    var post = item(0, images: images)
    let view = ShadowListUtilsFeedRowView(reuseIdentifier: nil)
    view.frame = CGRect(x: 0, y: 0, width: testWidth, height: 400)
    view.configure(row(post), layout: nil)
    view.layoutIfNeeded()
    let strip = try XCTUnwrap(view.subviews.compactMap { $0 as? UIScrollView }.first)
    strip.contentOffset.x = 300
    // The same post with a new text, like a later age, keeps the strip where it is.
    post.text = "Edited."
    view.configure(row(post), layout: nil)
    view.layoutIfNeeded()
    XCTAssertEqual(strip.contentOffset.x, 300)
    view.configure(row(item(1, images: images)), layout: nil)
    view.layoutIfNeeded()
    XCTAssertEqual(strip.contentOffset.x, -4)
  }

  func testAPlaceholderRowReadsAsLoadingAndKeepsTheRowsHeight() throws {
    var loading = item(0, text: String(repeating: "word ", count: 30), images: [ShadowListUtilsFeedImage(url: "a", width: 400, height: 300)])
    let height = row(loading).layout(width: testWidth).height
    loading.placeholder = true
    let placeholder = row(loading)
    XCTAssertNil(placeholder.time)
    XCTAssertEqual(placeholder.layout(width: testWidth).height, height)
    let view = ShadowListUtilsFeedRowView(reuseIdentifier: nil)
    view.configure(placeholder, layout: nil)
    XCTAssertEqual((view.accessibilityElements as? [NSObject])?.map(\.accessibilityLabel), ["Loading post"])
    view.configure(row(item(1)), layout: nil)
    XCTAssertEqual((view.accessibilityElements as? [NSObject])?.map(\.accessibilityLabel), ["Ada Lovelace, @ada, 3h, A short post."])
  }

  func testARightToLeftRowMirrorsItsFrames() throws {
    let images = (0..<3).map { ShadowListUtilsFeedImage(url: "https://example.com/\($0).jpg") }
    let post = row(item(0, images: images))
    let plain = try XCTUnwrap(post.layout(width: testWidth) as? ShadowListUtilsFeedRowLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { post.layout(width: testWidth) } as? ShadowListUtilsFeedRowLayout)
    XCTAssertEqual(mirrored.avatarFrame.minX, testWidth - plain.avatarFrame.maxX)
    XCTAssertEqual(mirrored.nameFrame.minX, testWidth - plain.nameFrame.maxX)
    XCTAssertEqual(mirrored.timeFrame.maxX, testWidth - plain.timeFrame.minX)
    XCTAssertEqual(mirrored.separatorFrame.maxX, testWidth - plain.separatorFrame.minX)
    XCTAssertEqual(mirrored.stripImageFrames.first?.maxX, 3 * 288 - 4)
    XCTAssertEqual(mirrored.height, plain.height)
  }
}
