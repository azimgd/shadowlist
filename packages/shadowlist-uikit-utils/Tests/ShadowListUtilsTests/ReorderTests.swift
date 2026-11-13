@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class ReorderTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  private func tile(_ index: Int) -> ShadowListUtilsReorderTileItem {
    ShadowListUtilsReorderTileItem(key: "tile-\(index)", title: "Tile \(index)", label: "\(index)")
  }

  func testTheListShowsItsItems() {
    let list = ShadowListUtilsReorderList(theme: theme)
    host(list)
    list.items = Fixture.contacts(5)
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<5).map { "contact-\($0)" })
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsReorderRowView)
  }

  func testTheListAndTheGridReorderAndTheGridHasThreeColumns() {
    let list = ShadowListUtilsReorderList(theme: theme)
    XCTAssertTrue(list.reorderEnabled)
    XCTAssertEqual(list.numberOfColumns, 1)
    XCTAssertFalse(list.allowsSelection)
    let grid = ShadowListUtilsReorderGrid(theme: theme)
    XCTAssertTrue(grid.reorderEnabled)
    XCTAssertEqual(grid.numberOfColumns, 3)
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() {
    let list = ShadowListUtilsReorderList(theme: theme)
    host(list)
    var items = Fixture.contacts(4)
    list.items = items
    list.layoutIfNeeded()
    let untouched = list.rowView(at: 0)
    items[2].name = "Renamed"
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 2)?.row as? ShadowListUtilsReorderRow)?.item.name, "Renamed")
    XCTAssertTrue(list.rowView(at: 0) === untouched)
    items.removeLast()
    list.setItems(items, animated: true)
    XCTAssertEqual(list.keys.count, 3)
  }

  func testADropMovesTheItemAndRunsOnMoveItemWithTheItemsAfterTheMove() {
    let list = ShadowListUtilsReorderList(theme: theme)
    host(list)
    list.items = Fixture.contacts(5)
    var moves: [(Int, Int, [String])] = []
    list.onMoveItem = { moves.append(($0, $1, $2.map(\.key))) }
    list.drop(from: 1, to: 3)
    XCTAssertEqual(moves.count, 1)
    XCTAssertEqual(moves[0].0, 1)
    XCTAssertEqual(moves[0].1, 3)
    XCTAssertEqual(moves[0].2, ["contact-0", "contact-2", "contact-3", "contact-1", "contact-4"])
    XCTAssertEqual(list.items.map(\.key), moves[0].2)
    XCTAssertEqual(list.keys, moves[0].2)
  }

  func testADropInPlaceDoesNotRunOnMoveItem() {
    let list = ShadowListUtilsReorderList(theme: theme)
    let grid = ShadowListUtilsReorderGrid(theme: theme)
    host(list)
    list.items = Fixture.contacts(3)
    grid.items = (0..<3).map(tile)
    var moves = 0
    list.onMoveItem = { _, _, _ in moves += 1 }
    grid.onMoveItem = { _, _, _ in moves += 1 }
    list.drop(from: 1, to: 1)
    grid.drop(from: 2, to: 2)
    XCTAssertEqual(moves, 0)
    XCTAssertEqual(list.keys, ["contact-0", "contact-1", "contact-2"])
    list.drop(from: 1, to: 0)
    XCTAssertEqual(moves, 1)
  }

  func testAScreenReaderMovesARowByOnePlace() throws {
    let list = ShadowListUtilsReorderList(theme: theme)
    host(list)
    list.items = Fixture.contacts(3)
    list.layoutIfNeeded()
    let view = try XCTUnwrap(list.rowView(at: 0))
    XCTAssertNil(view.accessibilityCustomActions)
    XCTAssertEqual(view.accessibilityLabel, "Person 0, Seat 0")
    XCTAssertEqual(view.accessibilityHint, "Press and hold, then drag to reorder")
    var moved: [String] = []
    list.onMoveItem = { _, _, items in moved = items.map(\.key) }
    let actions = try XCTUnwrap(view.accessibilityCustomActions)
    XCTAssertEqual(actions.map(\.name), ["Move up", "Move down"])
    XCTAssertEqual(actions[0].actionHandler?(actions[0]), false)
    XCTAssertEqual(actions[1].actionHandler?(actions[1]), true)
    XCTAssertEqual(moved, ["contact-1", "contact-0", "contact-2"])
    XCTAssertEqual(list.keys, moved)
  }

  func testTheGridMovesTilesAndNamesItsActionsEarlierAndLater() throws {
    let grid = ShadowListUtilsReorderGrid(theme: theme)
    host(grid)
    grid.items = (0..<6).map(tile)
    grid.layoutIfNeeded()
    XCTAssertTrue(grid.rowView(at: 0) is ShadowListUtilsReorderTileView)
    var moves: [[String]] = []
    grid.onMoveItem = { _, _, items in moves.append(items.map(\.key)) }
    grid.drop(from: 0, to: 5)
    XCTAssertEqual(moves.last, ["tile-1", "tile-2", "tile-3", "tile-4", "tile-5", "tile-0"])
    XCTAssertEqual(grid.items.map(\.key), moves.last)
    // The kit reads the data again after a drop of its own.
    grid.reloadData()
    grid.layoutIfNeeded()
    let view = try XCTUnwrap(grid.rowView(at: 0))
    XCTAssertEqual(view.accessibilityCustomActions?.map(\.name), ["Move earlier", "Move later"])
    XCTAssertEqual(view.accessibilityLabel, "1, Tile 1")
  }

  func testTheRowLayoutHoldsItsMetrics() throws {
    let row = ShadowListUtilsReorderRow(item: Fixture.contact(0), theme: theme, labels: ShadowListUtilsReorderLabels())
    let layout = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsReorderRowLayout)
    XCTAssertEqual(layout.height, 67)
    XCTAssertEqual(layout.avatarFrame, CGRect(x: 16, y: 13.5, width: 40, height: 40))
    XCTAssertEqual(layout.nameFrame.origin, CGPoint(x: 68, y: 12))
    XCTAssertEqual(layout.subtitleFrame.origin, CGPoint(x: 68, y: 35))
    XCTAssertEqual(layout.gripFrame, CGRect(x: testWidth - 16 - 20, y: ShadowListUtilsPlatform.roundToPixel(23.5), width: 20, height: 20))
    XCTAssertEqual(layout.separatorFrame.minX, 68)
    XCTAssertEqual(layout.separatorFrame.maxY, 67)
    XCTAssertEqual(layout.name.lineCount, 1)
  }

  func testTheTileLayoutHoldsItsMetrics() throws {
    let footnote = theme.typography.footnote
    let own = ShadowListUtilsReorderTile(item: tile(0), aspectRatio: nil, theme: theme, labels: ShadowListUtilsReorderLabels())
    let layout = try XCTUnwrap(own.layout(width: 130) as? ShadowListUtilsReorderTileLayout)
    XCTAssertEqual(layout.swatchFrame, CGRect(x: 6, y: 0, width: 118, height: 118))
    XCTAssertEqual(layout.labelFrame.midX, layout.swatchFrame.midX, accuracy: 1)
    XCTAssertEqual(layout.labelFrame.midY, layout.swatchFrame.midY, accuracy: 1)
    XCTAssertEqual(layout.titleFrame.origin, CGPoint(x: 10, y: 118 + 4))
    XCTAssertEqual(layout.height, 118 + 4 + footnote.scaledLineHeight() + 12)

    var long = tile(1)
    long.title = String(repeating: "word ", count: 20)
    long.aspectRatio = 2
    let wrapped = try XCTUnwrap(
      ShadowListUtilsReorderTile(item: long, aspectRatio: nil, theme: theme, labels: ShadowListUtilsReorderLabels()).layout(width: 130)
        as? ShadowListUtilsReorderTileLayout)
    XCTAssertEqual(wrapped.swatchFrame.height, 59)
    XCTAssertEqual(wrapped.title.lineCount, 2)
    let even = try XCTUnwrap(
      ShadowListUtilsReorderTile(item: long, aspectRatio: 1, theme: theme, labels: ShadowListUtilsReorderLabels()).layout(width: 130)
        as? ShadowListUtilsReorderTileLayout)
    XCTAssertEqual(even.swatchFrame.height, 118)
    XCTAssertEqual(even.title.lineCount, 1)
  }

  func testTileAspectRatioGivesEveryTileOfTheGridOneHeight() {
    let grid = ShadowListUtilsReorderGrid(theme: theme)
    host(grid)
    var items = (0..<6).map(tile)
    items[1].aspectRatio = 0.5
    grid.items = items
    grid.layoutIfNeeded()
    XCTAssertNotEqual(grid.rectForItem(at: 0).height, grid.rectForItem(at: 1).height)
    grid.tileAspectRatio = 1
    grid.layoutIfNeeded()
    XCTAssertEqual(grid.rectForItem(at: 0).height, grid.rectForItem(at: 1).height)
  }

  func testATileLabelKeepsItsSizeAndATitleTakesFourLinesAtALargeTextSize() throws {
    var long = tile(0)
    long.title = String(repeating: "word ", count: 30)
    let own = ShadowListUtilsReorderTile(item: long, aspectRatio: nil, theme: theme, labels: ShadowListUtilsReorderLabels())
    let even = ShadowListUtilsReorderTile(item: long, aspectRatio: 1, theme: theme, labels: ShadowListUtilsReorderLabels())
    let plain = try XCTUnwrap(own.layout(width: 130) as? ShadowListUtilsReorderTileLayout)
    try withTextSize(1.5) {
      let large = try XCTUnwrap(own.layout(width: 130) as? ShadowListUtilsReorderTileLayout)
      XCTAssertEqual(large.label?.size, plain.label?.size)
      XCTAssertEqual(large.title.lineCount, 4)
      assertLines(large.title, theme.typography.footnote, maxLines: 4)
      XCTAssertEqual((even.layout(width: 130) as? ShadowListUtilsReorderTileLayout)?.title.lineCount, 1)
    }
    XCTAssertEqual(plain.title.lineCount, 2)
  }

  func testARightToLeftRowMirrorsItsFrames() throws {
    let row = ShadowListUtilsReorderRow(item: Fixture.contact(0), theme: theme, labels: ShadowListUtilsReorderLabels())
    let plain = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsReorderRowLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { row.layout(width: testWidth) } as? ShadowListUtilsReorderRowLayout)
    XCTAssertEqual(mirrored.avatarFrame.minX, testWidth - plain.avatarFrame.maxX)
    XCTAssertEqual(mirrored.nameFrame.maxX, testWidth - 68)
    XCTAssertEqual(mirrored.gripFrame.minX, 16)
    let tile = ShadowListUtilsReorderTile(item: tile(0), aspectRatio: nil, theme: theme, labels: ShadowListUtilsReorderLabels())
    let plainTile = try XCTUnwrap(tile.layout(width: 130) as? ShadowListUtilsReorderTileLayout)
    let mirroredTile = try XCTUnwrap(withRightToLeft { tile.layout(width: 130) } as? ShadowListUtilsReorderTileLayout)
    XCTAssertEqual(mirroredTile.titleFrame.maxX, 130 - plainTile.titleFrame.minX)
  }
}
