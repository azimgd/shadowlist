import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class NestedTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  private func item(_ index: Int, cards: Int = 12) -> ShadowListUtilsNestedItem {
    ShadowListUtilsNestedItem(
      key: "shelf-\(index)", title: "Shelf \(index)",
      cards: (0..<cards).map {
        ShadowListUtilsNestedCardItem(key: "shelf-\(index)-card-\($0)", title: "Card \($0)", imageUrl: "https://example.com/\($0).jpg")
      })
  }

  private func nestedList(_ view: ShadowListUtilsRowView?) throws -> ShadowListUtilsList {
    try XCTUnwrap(view?.subviews.compactMap { $0 as? ShadowListUtilsList }.first)
  }

  func testAListShowsNestedRowsWithAHorizontalListOfCards() throws {
    let list = ShadowListUtilsList(theme: theme)
    host(list)
    list.setRows((0..<6).map { ShadowListUtilsNestedRow(item: item($0), theme: theme) })
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<6).map { "shelf-\($0)" })
    let view = try XCTUnwrap(list.rowView(at: 0))
    XCTAssertTrue(view is ShadowListUtilsNestedRowView)
    let nested = try nestedList(view)
    nested.layoutIfNeeded()
    XCTAssertTrue(nested.isHorizontal)
    XCTAssertEqual(nested.keys.count, 12)
    XCTAssertTrue(nested.rowView(at: 0) is ShadowListUtilsNestedCardView)
    XCTAssertEqual(nested.rectForItem(at: 1), CGRect(x: 196, y: 0, width: 196, height: 268))
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() throws {
    let list = ShadowListUtilsList(theme: theme)
    host(list)
    var items = (0..<2).map { item($0) }
    list.setRows(items.map { ShadowListUtilsNestedRow(item: $0, theme: theme) })
    list.layoutIfNeeded()
    items[1].cards.removeLast(4)
    list.setRows(items.map { ShadowListUtilsNestedRow(item: $0, theme: theme) })
    list.layoutIfNeeded()
    XCTAssertEqual(try nestedList(list.rowView(at: 1)).keys.count, 8)
    XCTAssertEqual(try nestedList(list.rowView(at: 0)).keys.count, 12)
    list.setRows([ShadowListUtilsNestedRow(item: items[1], theme: theme)], animated: true)
    XCTAssertEqual(list.keys, ["shelf-1"])
  }

  func testOnPressCardRunsWithTheCardAndItsRow() throws {
    let list = ShadowListUtilsList(theme: theme)
    host(list)
    var pressed: [String] = []
    list.setRows([ShadowListUtilsNestedRow(item: item(0), theme: theme) { card, row in pressed.append("\(row.key)/\(card.key)") }])
    list.layoutIfNeeded()
    let nested = try nestedList(list.rowView(at: 0))
    XCTAssertTrue(nested.allowsSelection)
    nested.tap(3)
    XCTAssertEqual(pressed, ["shelf-0/shelf-0-card-3"])

    list.setRows([ShadowListUtilsNestedRow(item: item(1), theme: theme)])
    list.layoutIfNeeded()
    XCTAssertFalse(try nestedList(list.rowView(at: 0)).allowsSelection)
  }

  func testANewOnPressCardReachesAMountedRow() throws {
    let list = ShadowListUtilsList(theme: theme)
    host(list)
    var pressed: [String] = []
    list.setRows([ShadowListUtilsNestedRow(item: item(0), theme: theme)])
    list.layoutIfNeeded()
    let view = try XCTUnwrap(list.rowView(at: 0))
    let nested = try nestedList(view)
    XCTAssertFalse(nested.allowsSelection)
    // The same item under the same content version, with a closure the row did not have.
    list.setRows([ShadowListUtilsNestedRow(item: item(0), theme: theme) { card, _ in pressed.append("first \(card.key)") }])
    XCTAssertTrue(list.rowView(at: 0) === view)
    XCTAssertTrue(nested.allowsSelection)
    nested.tap(1)
    list.setRows([ShadowListUtilsNestedRow(item: item(0), theme: theme) { card, _ in pressed.append("second \(card.key)") }])
    nested.tap(2)
    XCTAssertEqual(pressed, ["first shelf-0-card-1", "second shelf-0-card-2"])
  }

  func testTheRowLayoutHoldsItsMetrics() throws {
    let row = ShadowListUtilsNestedRow(item: item(0), theme: theme)
    let layout = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsNestedRowLayout)
    let cardHeight: CGFloat = 220 + 8 + 2 * 20
    XCTAssertEqual(layout.height, 25 + 12 + cardHeight + 16)
    XCTAssertEqual(layout.titleFrame.origin, CGPoint(x: 16, y: 0))
    assertLines(layout.title, theme.typography.title3, maxLines: 1)
    XCTAssertEqual(layout.listFrame, CGRect(x: 0, y: 25 + 12, width: testWidth, height: cardHeight))
  }

  func testTheRowGrowsWithTheCardsTitlesAtALargeTextSize() throws {
    let row = ShadowListUtilsNestedRow(item: item(0), theme: theme)
    let subhead = theme.typography.subhead
    let large = try XCTUnwrap(withTextSize(1.5) { row.layout(width: testWidth) } as? ShadowListUtilsNestedRowLayout)
    let lineHeight = withTextSize(1.5) { subhead.scaledLineHeight() }
    XCTAssertGreaterThan(lineHeight, subhead.scaledLineHeight())
    XCTAssertEqual(large.listFrame.height, 220 + 8 + 2 * lineHeight)
    XCTAssertEqual(large.height, large.listFrame.maxY + 16)
    // Two lines of a card's title end inside the list.
    let long = ShadowListUtilsNestedCardItem(key: "long", title: String(repeating: "word ", count: 20), imageUrl: "https://example.com/long.jpg")
    let card = ShadowListUtilsNestedCard(item: long, theme: theme)
    let cardLayout = try XCTUnwrap(withTextSize(1.5) { card.layout(width: large.listFrame.height) } as? ShadowListUtilsNestedCardLayout)
    XCTAssertEqual(cardLayout.title.lineCount, 2)
    XCTAssertEqual(cardLayout.height, large.listFrame.height)
  }

  func testTheCardLayoutHoldsItsMetrics() throws {
    let card = ShadowListUtilsNestedCard(item: item(0).cards[0], theme: theme)
    let layout = try XCTUnwrap(card.layout(width: 268) as? ShadowListUtilsNestedCardLayout)
    XCTAssertEqual(layout.width, 16 + 180)
    XCTAssertEqual(layout.imageFrame, CGRect(x: 16, y: 0, width: 180, height: 220))
    XCTAssertEqual(layout.titleFrame.origin, CGPoint(x: 16, y: 228))
    assertLines(layout.title, theme.typography.subhead, maxLines: 2)
    // The layout's height is the height of the card's content, not the height of the list.
    XCTAssertEqual(layout.height, layout.titleFrame.maxY)
  }

  func testANestedListKeepsItsScrollPositionWhileItsRowIsOffScreen() throws {
    let list = ShadowListUtilsList(theme: theme)
    host(list)
    list.setRows((0..<12).map { ShadowListUtilsNestedRow(item: item($0), theme: theme) })
    list.layoutIfNeeded()
    let nested = try nestedList(list.rowView(at: 0))
    nested.layoutIfNeeded()
    nested.scrollToItem(at: 5, viewPosition: 0, animated: false)
    nested.layoutIfNeeded()
    let offset = nested.contentOffset.x
    XCTAssertEqual(offset, 5 * 196)

    list.scrollToItem(at: 10, viewPosition: 0, animated: false)
    list.layoutIfNeeded()
    XCTAssertNil(list.rowView(at: 0))
    let other = try nestedList(list.rowView(at: 10))
    other.layoutIfNeeded()
    XCTAssertEqual(other.contentOffset.x, 0)

    list.scrollToStart(animated: false)
    list.layoutIfNeeded()
    let back = try nestedList(list.rowView(at: 0))
    back.layoutIfNeeded()
    XCTAssertEqual(back.contentOffset.x, offset)
  }

  func testARightToLeftRowMirrorsItsFrames() throws {
    let row = ShadowListUtilsNestedRow(item: item(0), theme: theme)
    let plain = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsNestedRowLayout)
    let card = ShadowListUtilsNestedCard(item: item(0).cards[0], theme: theme)
    try withRightToLeft {
      let mirrored = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsNestedRowLayout)
      XCTAssertEqual(mirrored.titleFrame.maxX, testWidth - 16)
      XCTAssertEqual(mirrored.titleFrame.minX, testWidth - plain.titleFrame.maxX)
      XCTAssertEqual(mirrored.listFrame, plain.listFrame)
      // A card keeps its margin on the side the list starts at.
      let cardLayout = try XCTUnwrap(card.layout(width: 268) as? ShadowListUtilsNestedCardLayout)
      XCTAssertEqual(cardLayout.imageFrame, CGRect(x: 0, y: 0, width: 180, height: 220))
    }
  }
}
