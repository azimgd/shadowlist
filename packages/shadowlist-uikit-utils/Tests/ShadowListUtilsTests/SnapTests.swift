@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class SnapTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  private func item(_ index: Int) -> ShadowListUtilsSnapItem {
    ShadowListUtilsSnapItem(key: "card-\(index)", title: "Lisbon", subtitle: "Portugal", imageUrl: "https://example.com/\(index).jpg")
  }

  func testTheListShowsItsItems() {
    let list = ShadowListUtilsSnapList(theme: theme)
    host(list)
    list.items = (0..<6).map(item)
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<6).map { "card-\($0)" })
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsSnapCardView)
  }

  func testTheListSnapsToTheStartOfAnItem() {
    let list = ShadowListUtilsSnapList(theme: theme)
    XCTAssertTrue(list.snapToItem)
    XCTAssertEqual(list.snapAlignment, .start)
    XCTAssertFalse(list.allowsSelection)
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() {
    let list = ShadowListUtilsSnapList(theme: theme)
    host(list)
    var items = (0..<3).map(item)
    list.items = items
    list.layoutIfNeeded()
    let untouched = list.rowView(at: 0)
    items[1].title = "Porto"
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 1)?.row as? ShadowListUtilsSnapCard)?.item.title, "Porto")
    XCTAssertTrue(list.rowView(at: 0) === untouched)
    items.remove(at: 0)
    list.setItems(items, animated: true)
    XCTAssertEqual(list.keys, ["card-1", "card-2"])
  }

  func testOnPressItemRunsForATappedCard() {
    let list = ShadowListUtilsSnapList(theme: theme)
    host(list)
    list.items = (0..<3).map(item)
    var pressed: [String] = []
    list.onPressItem = { pressed.append($0.key) }
    list.tap(0)
    XCTAssertEqual(pressed, ["card-0"])
  }

  func testACardIsAQuarterOfTheWindowUnlessCardHeightIsSet() throws {
    let list = ShadowListUtilsSnapList(theme: theme)
    host(list)
    list.items = (0..<3).map(item)
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 0).height, 200)
    XCTAssertEqual((list.rows.first as? ShadowListUtilsSnapCard)?.height, 200)
    // A window of another height sizes the cards again, on the pixel grid.
    let window = try XCTUnwrap(list.window)
    window.frame.size.height = 730
    list.frame = window.bounds
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 0).height, ShadowListUtilsPlatform.roundToPixel(182.5))
    list.cardHeight = 300
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 1), CGRect(x: 0, y: 300, width: testWidth, height: 300))
    // A card outside a snap list falls back to the height of the window the library last saw.
    let alone = ShadowListUtilsSnapCard(item: item(0), height: nil, theme: theme, labels: ShadowListUtilsSnapLabels())
    XCTAssertEqual(alone.layout(width: testWidth).height, ShadowListUtilsPlatform.roundToPixel(182.5))
  }

  func testANewCardHeightLaysEveryCardOutAgain() {
    let list = ShadowListUtilsSnapList(theme: theme)
    host(list)
    list.cardHeight = 240
    list.items = (0..<3).map(item)
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 2).minY, 480)
    list.cardHeight = 120
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 2), CGRect(x: 0, y: 240, width: testWidth, height: 120))
    XCTAssertEqual(list.rowView(at: 0)?.frame.height, 120)
  }

  func testTheCardLayoutHoldsItsMetrics() throws {
    let card = ShadowListUtilsSnapCard(item: item(0), height: 240, theme: theme, labels: ShadowListUtilsSnapLabels())
    let layout = try XCTUnwrap(card.layout(width: testWidth) as? ShadowListUtilsSnapCardLayout)
    XCTAssertEqual(layout.height, 240)
    XCTAssertEqual(layout.cardFrame, CGRect(x: 8, y: 8, width: testWidth - 16, height: 224))
    let title = try XCTUnwrap(layout.title)
    let subtitle = try XCTUnwrap(layout.subtitle)
    assertLines(title, theme.typography.title3, maxLines: 2)
    assertLines(subtitle, theme.typography.subhead, maxLines: 2)
    let captionHeight = 16 + title.size.height + subtitle.size.height + 16
    XCTAssertEqual(layout.captionFrame, CGRect(x: 0, y: 224 - captionHeight, width: testWidth - 16, height: captionHeight))
    XCTAssertEqual(layout.titleFrame.origin, CGPoint(x: 16, y: 16))
    XCTAssertEqual(layout.subtitleFrame.origin, CGPoint(x: 16, y: 16 + title.size.height))

    let bare = ShadowListUtilsSnapCard(item: ShadowListUtilsSnapItem(key: "bare"), height: 240, theme: theme, labels: ShadowListUtilsSnapLabels())
    XCTAssertEqual((bare.layout(width: testWidth) as? ShadowListUtilsSnapCardLayout)?.captionFrame, .zero)
  }

  func testARightToLeftCardMirrorsItsFrames() throws {
    let card = ShadowListUtilsSnapCard(item: item(0), height: 240, theme: theme, labels: ShadowListUtilsSnapLabels())
    let plain = try XCTUnwrap(card.layout(width: testWidth) as? ShadowListUtilsSnapCardLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { card.layout(width: testWidth) } as? ShadowListUtilsSnapCardLayout)
    XCTAssertEqual(mirrored.cardFrame, plain.cardFrame)
    XCTAssertEqual(mirrored.captionFrame, plain.captionFrame)
    // The text frames mirror inside the card, which is narrower than the row.
    XCTAssertEqual(mirrored.titleFrame.maxX, plain.cardFrame.width - 16, accuracy: 1e-9)
    XCTAssertEqual(mirrored.subtitleFrame.minX, plain.cardFrame.width - plain.subtitleFrame.maxX)
  }

  func testTheCardReadsItsTextItsAltOrTheLabel() {
    let view = ShadowListUtilsSnapCardView(reuseIdentifier: nil)
    let labels = ShadowListUtilsSnapLabels()
    view.configure(ShadowListUtilsSnapCard(item: item(0), height: 240, theme: theme, labels: labels), layout: nil)
    XCTAssertEqual(view.accessibilityLabel, "Lisbon, Portugal")
    view.configure(
      ShadowListUtilsSnapCard(item: ShadowListUtilsSnapItem(key: "alt", imageAlt: "A tram"), height: 240, theme: theme, labels: labels), layout: nil)
    XCTAssertEqual(view.accessibilityLabel, "A tram")
    view.configure(ShadowListUtilsSnapCard(item: ShadowListUtilsSnapItem(key: "bare"), height: 240, theme: theme, labels: labels), layout: nil)
    XCTAssertEqual(view.accessibilityLabel, "Card")
  }
}
