@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class MasonryTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  private func item(_ index: Int, title: String? = "Photo title") -> ShadowListUtilsMasonryItem {
    ShadowListUtilsMasonryItem(
      key: "photo-\(index)", image: ShadowListUtilsMasonryImage(url: "https://example.com/\(index).jpg", width: 400, height: 600), title: title)
  }

  func testTheListShowsItsItems() {
    let list = ShadowListUtilsMasonryList(theme: theme)
    host(list)
    list.items = (0..<6).map { item($0) }
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<6).map { "photo-\($0)" })
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsMasonryCardView)
    XCTAssertEqual(list.items.count, 6)
  }

  func testTheListIsAGridOfThreeColumnsThatStackTheirOwnCards() {
    let list = ShadowListUtilsMasonryList(theme: theme)
    XCTAssertEqual(list.numberOfColumns, 3)
    XCTAssertFalse(list.allowsSelection)
    host(list)
    list.items = (0..<6).map { item($0) }
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 0).width, testWidth / 3)
    XCTAssertEqual(list.rectForItem(at: 1).minX, testWidth / 3)
    XCTAssertEqual(list.rectForItem(at: 3).minY, list.rectForItem(at: 0).maxY)
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() {
    let list = ShadowListUtilsMasonryList(theme: theme)
    host(list)
    var items = (0..<6).map { item($0) }
    list.items = items
    list.layoutIfNeeded()
    let untouched = list.rowView(at: 0)
    items[1].title = "A new title"
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 1)?.row as? ShadowListUtilsMasonryCard)?.item.title, "A new title")
    XCTAssertTrue(list.rowView(at: 0) === untouched)
    items.remove(at: 2)
    list.setItems(items, animated: true)
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, ["photo-0", "photo-1", "photo-3", "photo-4", "photo-5"])
    XCTAssertEqual(list.visibleCells.count, 5)
  }

  func testOnPressItemRunsForATappedCardAndLeavesNothingSelected() {
    let list = ShadowListUtilsMasonryList(theme: theme)
    host(list)
    list.items = (0..<3).map { item($0) }
    var pressed: [String] = []
    var selectedWhilePressed: [Int] = []
    list.onPressItem = { [unowned list] in
      pressed.append($0.key)
      selectedWhilePressed = Array(list.selectedIndices)
    }
    XCTAssertTrue(list.allowsSelection)
    // The kit selects the item, then tells the list. The press runs before the item deselects.
    list.selectItem(at: 2, animated: false)
    list.tap(2)
    XCTAssertEqual(pressed, ["photo-2"])
    XCTAssertEqual(selectedWhilePressed, [2])
    XCTAssertEqual(list.selectedIndices.count, 0)
    list.onPressItem = nil
    XCTAssertFalse(list.allowsSelection)
  }

  func testRowForItemReplacesTheCard() {
    let list = ShadowListUtilsMasonryList(theme: theme)
    host(list)
    list.items = (0..<3).map { item($0) }
    list.rowForItem = { CountingRow(key: $0.key) }
    list.layoutIfNeeded()
    XCTAssertTrue(list.rowView(at: 0) is CountingRowView)
    XCTAssertEqual(list.rectForItem(at: 0).height, 44)
  }

  func testTheCardLayoutHoldsItsMetrics() throws {
    let card = ShadowListUtilsMasonryCard(item: item(0), theme: theme, labels: ShadowListUtilsMasonryLabels())
    let layout = try XCTUnwrap(card.layout(width: 130) as? ShadowListUtilsMasonryCardLayout)
    XCTAssertEqual(layout.imageFrame, CGRect(x: 6, y: 0, width: 118, height: 177))
    XCTAssertEqual(layout.titleFrame.origin, CGPoint(x: 10, y: 177 + 8))
    let title = try XCTUnwrap(layout.title)
    assertLines(title, theme.typography.subhead, maxLines: 2)
    XCTAssertEqual(layout.height, 177 + 8 + title.size.height + 12)

    let untitled = ShadowListUtilsMasonryCard(item: item(1, title: nil), theme: theme, labels: ShadowListUtilsMasonryLabels())
    XCTAssertEqual(untitled.layout(width: 130).height, 177 + 8 + 12)
    var square = item(2, title: nil)
    square.image.height = 0
    let squareLayout = try XCTUnwrap(
      ShadowListUtilsMasonryCard(item: square, theme: theme, labels: ShadowListUtilsMasonryLabels()).layout(width: 130) as? ShadowListUtilsMasonryCardLayout)
    XCTAssertEqual(squareLayout.imageFrame.size, CGSize(width: 118, height: 118))
  }

  func testATitleTakesFourLinesAtALargeTextSize() throws {
    let card = ShadowListUtilsMasonryCard(
      item: item(0, title: String(repeating: "A long title. ", count: 20)), theme: theme, labels: ShadowListUtilsMasonryLabels())
    XCTAssertEqual((card.layout(width: 130) as? ShadowListUtilsMasonryCardLayout)?.title?.lineCount, 2)
    XCTAssertEqual(withTextSize(1.29) { (card.layout(width: 130) as? ShadowListUtilsMasonryCardLayout)?.title?.lineCount }, 2)
    XCTAssertEqual(withTextSize(1.3) { (card.layout(width: 130) as? ShadowListUtilsMasonryCardLayout)?.title?.lineCount }, 4)
  }

  func testARightToLeftCardMirrorsItsFrames() throws {
    let card = ShadowListUtilsMasonryCard(item: item(0, title: "Hi"), theme: theme, labels: ShadowListUtilsMasonryLabels())
    let plain = try XCTUnwrap(card.layout(width: 130) as? ShadowListUtilsMasonryCardLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { card.layout(width: 130) } as? ShadowListUtilsMasonryCardLayout)
    XCTAssertEqual(mirrored.imageFrame, plain.imageFrame)
    XCTAssertEqual(mirrored.titleFrame.maxX, 130 - 10)
    XCTAssertEqual(mirrored.titleFrame.minX, 130 - plain.titleFrame.maxX)
  }

  func testTheCardReadsItsAltItsTitleOrTheLabel() {
    let view = ShadowListUtilsMasonryCardView(reuseIdentifier: nil)
    var labels = ShadowListUtilsMasonryLabels()
    XCTAssertEqual(labels.image, "Photo")
    labels.image = "Picture"
    var photo = item(0, title: nil)
    view.configure(ShadowListUtilsMasonryCard(item: photo, theme: theme, labels: labels), layout: nil)
    XCTAssertEqual(view.accessibilityLabel, "Picture")
    photo.title = "Harbor"
    view.configure(ShadowListUtilsMasonryCard(item: photo, theme: theme, labels: labels), layout: nil)
    XCTAssertEqual(view.accessibilityLabel, "Harbor")
    photo.image.alt = "Boats at dusk"
    view.configure(ShadowListUtilsMasonryCard(item: photo, theme: theme, labels: labels), layout: nil)
    XCTAssertEqual(view.accessibilityLabel, "Boats at dusk")
    XCTAssertTrue(view.accessibilityTraits.contains(.image))
  }
}
