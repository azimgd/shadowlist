import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * The list as accessibility sees it: a table of every row, mounted or not, with the mounted cell
 * in its row, the selected rows, scroll to visible, the notifications, the scroll bars, the move
 * actions and a list hidden for a host.
 */
@MainActor
final class AccessibilityTests: XCTestCase {
  /*
   * Moves the keys of its source and records the selection.
   */
  private final class Host: NSObject, ShadowListKitListViewDelegate {
    let source: TestSource
    var moves: [[Int]] = []
    var fixed: Set<Int> = []
    var selected: [Int] = []

    init(source: TestSource) {
      self.source = source
    }

    func listView(_ listView: ShadowListKitListView, didSelectItemAt index: Int) { selected.append(index) }
    func listView(_ listView: ShadowListKitListView, canMoveItemAt index: Int) -> Bool { !fixed.contains(index) }

    func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
      moves.append([sourceIndex, destinationIndex])
      let key = source.keys.remove(at: sourceIndex)
      source.keys.insert(key, at: destinationIndex)
    }
  }

  /*
   * Three sections of four 44 point items under a 30 point titled header.
   */
  private final class Sections: TestSource {
    func numberOfSections(in listView: ShadowListKitListView) -> Int { 3 }
    func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { 4 }
    func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { "Section \(section)" }
    func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat { 30 }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 44 }
  }

  private static let scrollToVisible = "AXScrollToVisible"

  /*
   * 600 items of 44 points in a 320 by 480 list, with a host that moves them.
   */
  private func makeHarness(
    count: Int = 600, configure: (ShadowListKitListView) -> Void = { _ in }
  ) -> (ListHarness, Host) {
    let source = SizedTestSource(count: count)
    let harness = ListHarness(source: source, configure: configure)
    let host = Host(source: source)
    harness.list.delegate = host
    return (harness, host)
  }

  private func rows(_ list: ShadowListKitListView) -> [NSAccessibilityElement] {
    (list.accessibilityRows() ?? []).compactMap { $0 as? NSAccessibilityElement }
  }

  private func row(_ list: ShadowListKitListView, _ index: Int) throws -> NSAccessibilityElement {
    let all = rows(list)
    XCTAssertLessThan(index, all.count)
    return all[index]
  }

  /*
   * A rect in the list's coordinates on the screen, the way accessibility gives frames.
   */
  private func screenRect(_ list: ShadowListKitListView, _ rect: NSRect) throws -> NSRect {
    try XCTUnwrap(list.window).convertToScreen(list.convert(rect, to: nil))
  }

  private func cellFrame(_ list: ShadowListKitListView, _ cell: ShadowListKitListCell) throws -> NSRect {
    try screenRect(list, list.convert(cell.frame, from: cell.superview))
  }

  private func record(_ list: ShadowListKitListView) -> () -> [(NSAccessibility.Notification, [AnyHashable: Any]?)] {
    var posted: [(NSAccessibility.Notification, [AnyHashable: Any]?)] = []
    list.shadowListKit_accessibilityNotificationHandler = { name, userInfo in
      posted.append((name, userInfo))
    }
    return { posted }
  }

  private func names(_ posted: [(NSAccessibility.Notification, [AnyHashable: Any]?)]) -> [NSAccessibility.Notification] {
    posted.map { $0.0 }
  }

  private func spin(_ seconds: TimeInterval = 0.05) {
    RunLoop.main.run(until: Date().addingTimeInterval(seconds))
  }

  private func scrollToVisible(_ element: NSAccessibilityElement) {
    _ = element.perform(NSSelectorFromString("accessibilityPerformAction:"), with: Self.scrollToVisible)
  }

  func testTheListIsATableOfEveryRowMountedOrNot() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    XCTAssertTrue(list.isAccessibilityElement())
    XCTAssertEqual(list.accessibilityRole(), .table)
    XCTAssertEqual(list.accessibilityRowCount(), 600)
    XCTAssertEqual(list.accessibilityColumnCount(), 1)
    let all = rows(list)
    XCTAssertEqual(all.count, 600)
    XCTAssertTrue(all.allSatisfy { $0.accessibilityRole() == .row })
    XCTAssertEqual(all[500].accessibilityIndex(), 500)
    XCTAssertTrue(all[500].accessibilityParent() as? ShadowListKitListView === list)
    // A mounted row holds its cell, a row far away holds nothing.
    let cell = try XCTUnwrap(list.cellForItem(at: 2))
    XCTAssertEqual(cell.accessibilityRole(), .cell)
    XCTAssertTrue(all[2].accessibilityChildren()?.first as? ShadowListKitListCell === cell)
    XCTAssertTrue(cell.accessibilityParent() as? NSAccessibilityElement === all[2])
    XCTAssertEqual(all[500].accessibilityChildren()?.count, 0)
    // An element follows its key while accessibility holds it.
    XCTAssertTrue(try row(list, 500) === all[500])
    // The rows come first, then the scroll bar.
    let children = list.accessibilityChildren() ?? []
    XCTAssertEqual(children.count, 601)
    XCTAssertEqual((children.last as? NSAccessibilityElement)?.accessibilityRole(), .scrollBar)
  }

  func testTheVisibleRowsAreTheRowsOverTheViewport() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let visible = (list.accessibilityVisibleRows() ?? []).compactMap { $0 as? NSAccessibilityElement }
    XCTAssertEqual(visible.map { $0.accessibilityIndex() }, Array(0..<11))
    list.contentOffset = CGPoint(x: 0, y: 44 * 100 + 10)
    harness.layout()
    let scrolled = (list.accessibilityVisibleRows() ?? []).compactMap { $0 as? NSAccessibilityElement }
    XCTAssertEqual(scrolled.first?.accessibilityIndex(), 100)
    XCTAssertEqual(scrolled.last?.accessibilityIndex(), 111)
  }

  func testARowFramesItselfOnTheScreen() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let cell = try XCTUnwrap(list.cellForItem(at: 3))
    XCTAssertEqual(try row(list, 3).accessibilityFrame(), try cellFrame(list, cell))
    // A row far away frames where the core places it.
    let far = try row(list, 500)
    let rect = list.rectForItem(at: 500)
    XCTAssertEqual(far.accessibilityFrame(), try screenRect(list, rect))
    XCTAssertEqual(far.accessibilityFrame().height, 44)
  }

  func testScrollToVisibleMountsTheRowAndPointsAccessibilityAtIt() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let posted = record(list)
    let far = try row(list, 300)
    scrollToVisible(far)
    let cell = try XCTUnwrap(list.cellForItem(at: 300))
    XCTAssertTrue(far.accessibilityChildren()?.first as? ShadowListKitListCell === cell)
    XCTAssertEqual(far.accessibilityFrame(), try cellFrame(list, cell))
    let frame = list.convert(cell.frame, from: cell.superview)
    XCTAssertGreaterThanOrEqual(frame.minY, -0.5)
    XCTAssertLessThanOrEqual(frame.maxY, list.bounds.height + 0.5)
    let layout = try XCTUnwrap(posted().last)
    XCTAssertEqual(layout.0, .layoutChanged)
    XCTAssertTrue((layout.1?[NSAccessibility.NotificationUserInfoKey.uiElements] as? [Any])?.first as? NSAccessibilityElement === far)
    // A row on screen stays where it is.
    let offset = list.contentOffset
    scrollToVisible(try row(list, 299))
    XCTAssertEqual(list.contentOffset, offset)
  }

  func testTheSelectedRowsFollowTheSelectionAndSelectLikeTheReader() throws {
    let (harness, host) = makeHarness { $0.allowsMultipleSelection = true }
    let list = harness.list
    XCTAssertEqual(list.accessibilitySelectedRows()?.count, 0)
    list.shadowListKit_clickItem(at: 3, modifierFlags: [], clickCount: 1)
    let selected = (list.accessibilitySelectedRows() ?? []).compactMap { $0 as? NSAccessibilityElement }
    XCTAssertEqual(selected.map { $0.accessibilityIndex() }, [3])
    XCTAssertTrue(try row(list, 3).isAccessibilitySelected())
    XCTAssertTrue(list.isAccessibilitySelectorAllowed(#selector(NSView.setAccessibilitySelectedRows(_:))))
    list.setAccessibilitySelectedRows([try row(list, 5), try row(list, 400)])
    XCTAssertEqual(Array(list.selectedIndices), [5, 400])
    XCTAssertEqual(host.selected, [3, 5, 400])
    // A row element selects and deselects itself.
    try row(list, 7).setAccessibilitySelected(true)
    try row(list, 5).setAccessibilitySelected(false)
    XCTAssertEqual(Array(list.selectedIndices), [7, 400])
    // Without multiple selection one row stays.
    list.allowsMultipleSelection = false
    list.setAccessibilitySelectedRows([try row(list, 8), try row(list, 9)])
    XCTAssertEqual(Array(list.selectedIndices), [8])
    // Without selection nothing selects.
    list.allowsSelection = false
    XCTAssertFalse(list.isAccessibilitySelectorAllowed(#selector(NSView.setAccessibilitySelectedRows(_:))))
    list.setAccessibilitySelectedRows([try row(list, 2)])
    try row(list, 2).setAccessibilitySelected(true)
    XCTAssertTrue(list.selectedIndices.isEmpty)
  }

  func testSelectionAndDataChangesPostTheirNotifications() throws {
    let (harness, _) = makeHarness { $0.allowsMultipleSelection = true }
    let list = harness.list
    let posted = record(list)
    // A click that deselects one row and selects another posts once.
    list.shadowListKit_clickItem(at: 1, modifierFlags: [], clickCount: 1)
    spin()
    list.shadowListKit_clickItem(at: 2, modifierFlags: [], clickCount: 1)
    spin()
    XCTAssertEqual(names(posted()), [.selectedRowsChanged, .selectedRowsChanged])
    list.selectItem(at: 2, animated: false)
    spin()
    XCTAssertEqual(posted().count, 2)
    list.deselectItem(at: 2, animated: false)
    spin()
    XCTAssertEqual(names(posted()).last, .selectedRowsChanged)
    XCTAssertEqual(posted().count, 3)
    // A data change posts the new row count, a reload with the same count nothing.
    harness.source.keys.insert("new", at: 0)
    list.insertItems(at: IndexSet(integer: 0))
    XCTAssertEqual(names(posted()).last, .rowCountChanged)
    XCTAssertEqual(posted().count, 4)
    list.reloadData()
    XCTAssertEqual(posted().count, 4)
  }

  func testTheScrollBarScrollsAPageAndToAValue() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let posted = record(list)
    let bars = (list.shadowListKit_scrollerView.accessibilityChildren() ?? []).compactMap { $0 as? NSAccessibilityElement }
    XCTAssertEqual(bars.count, 1)
    let bar = try XCTUnwrap(bars.first)
    XCTAssertEqual(bar.accessibilityRole(), .scrollBar)
    XCTAssertEqual(bar.accessibilityOrientation(), .vertical)
    XCTAssertEqual(bar.accessibilityValue() as? Double, 0)
    XCTAssertTrue(bar.isAccessibilitySelectorAllowed(#selector(NSAccessibilityElement.setAccessibilityValue(_:))))
    XCTAssertFalse(bar.accessibilityPerformDecrement())
    XCTAssertTrue(bar.accessibilityPerformIncrement())
    XCTAssertEqual(list.contentOffset.y, 480, accuracy: 0.5)
    let maxOffset = list.contentSize.height - 480
    XCTAssertEqual(bar.accessibilityValue() as? Double ?? -1, Double(480 / maxOffset), accuracy: 0.0001)
    // The list says which rows show.
    XCTAssertEqual(names(posted()), [.layoutChanged, .announcementRequested])
    let announcement = posted().last?.1?[NSAccessibility.NotificationUserInfoKey.announcement] as? String
    XCTAssertEqual(announcement, "Rows 11 to 22 of 600")
    XCTAssertNotNil(list.cellForItem(at: 12))
    bar.setAccessibilityValue(1.0)
    XCTAssertEqual(list.contentOffset.y, maxOffset, accuracy: 0.5)
    XCTAssertFalse(bar.accessibilityPerformIncrement())
    XCTAssertTrue(bar.accessibilityPerformDecrement())
    XCTAssertEqual(list.contentOffset.y, maxOffset - 480, accuracy: 0.5)
  }

  func testAHorizontalListHasAHorizontalScrollBarAndNoneWhenItFits() throws {
    let (harness, _) = makeHarness { $0.isHorizontal = true }
    let bars = (harness.list.shadowListKit_scrollerView.accessibilityChildren() ?? []).compactMap { $0 as? NSAccessibilityElement }
    XCTAssertEqual(bars.map { $0.accessibilityOrientation() }, [.horizontal])
    let (short, _) = makeHarness(count: 3)
    XCTAssertEqual(short.list.shadowListKit_scrollerView.accessibilityChildren()?.count, 0)
  }

  func testTheMoveActionsMoveTheItemTheWayADropDoes() throws {
    let (harness, host) = makeHarness { $0.reorderEnabled = true }
    let list = harness.list
    let key = harness.source.keys[2]
    let element = try row(list, 2)
    XCTAssertEqual(element.accessibilityCustomActions()?.map { $0.name }, ["Move up", "Move down"])
    XCTAssertEqual(try row(list, 0).accessibilityCustomActions()?.map { $0.name }, ["Move down"])
    XCTAssertEqual(try row(list, 599).accessibilityCustomActions()?.map { $0.name }, ["Move up"])
    let posted = record(list)
    let down = try XCTUnwrap(element.accessibilityCustomActions()?.last?.handler)
    XCTAssertTrue(down())
    XCTAssertEqual(host.moves, [[2, 3]])
    XCTAssertEqual(harness.source.keys[3], key)
    // The element follows the item.
    XCTAssertEqual(element.accessibilityIndex(), 3)
    XCTAssertTrue(try row(list, 3) === element)
    XCTAssertEqual(names(posted()).last, .layoutChanged)
    let up = try XCTUnwrap(element.accessibilityCustomActions()?.first?.handler)
    XCTAssertTrue(up())
    XCTAssertEqual(host.moves, [[2, 3], [3, 2]])
    // Items the delegate keeps, and a list without reorder, have no actions.
    host.fixed = [4]
    XCTAssertEqual(try row(list, 4).accessibilityCustomActions()?.count, 0)
    list.reorderEnabled = false
    XCTAssertEqual(try row(list, 2).accessibilityCustomActions()?.count, 0)
  }

  func testAGridMovesAcrossItsColumnsAndARowOfColumns() throws {
    let (harness, host) = makeHarness {
      $0.reorderEnabled = true
      $0.numberOfColumns = 3
    }
    let element = try row(harness.list, 4)
    let actions = element.accessibilityCustomActions() ?? []
    XCTAssertEqual(actions.map { $0.name }, ["Move left", "Move right", "Move up", "Move down"])
    XCTAssertTrue(try XCTUnwrap(actions.last?.handler)())
    XCTAssertEqual(host.moves, [[4, 7]])
  }

  func testInASectionedListTitlesAreRowsAndItemsMoveInsideTheirSection() throws {
    let source = Sections(count: 12)
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 0, height: 40))
    header.setAccessibilityElement(true)
    header.setAccessibilityRole(.group)
    let harness = ListHarness(source: source) {
      $0.reorderEnabled = true
      $0.headerView = header
    }
    let host = Host(source: source)
    harness.list.delegate = host
    let list = harness.list
    // Each section's title row leads its items, after the header view.
    XCTAssertEqual(list.accessibilityRowCount(), 15)
    let children = list.accessibilityChildren() ?? []
    XCTAssertTrue(children.first as? NSView === header)
    let title = try row(list, 0)
    let titleCell = try XCTUnwrap(title.accessibilityChildren()?.first as? ShadowListKitListCell)
    XCTAssertEqual(titleCell.index, NSNotFound)
    XCTAssertEqual(title.accessibilityCustomActions()?.count, 0)
    title.setAccessibilitySelected(true)
    XCTAssertTrue(list.selectedIndices.isEmpty)
    // The last item of the first section moves up only.
    XCTAssertEqual(try row(list, 4).accessibilityCustomActions()?.map { $0.name }, ["Move up"])
    XCTAssertEqual(try row(list, 1).accessibilityCustomActions()?.map { $0.name }, ["Move down"])
  }

  func testACellFadingOutAfterADeleteIsLeftOut() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    list.animatesChanges = true
    let posted = record(list)
    let fading = try XCTUnwrap(list.cellForItem(at: 2))
    let frame = try cellFrame(list, fading)
    harness.source.keys.remove(at: 2)
    list.deleteItems(at: IndexSet(integer: 2))
    harness.layout()
    XCTAssertFalse(fading.isHidden)
    XCTAssertEqual(list.accessibilityRowCount(), 599)
    XCTAssertEqual(names(posted()), [.rowCountChanged])
    for element in rows(list).prefix(12) {
      XCTAssertFalse((element.accessibilityChildren() ?? []).contains { $0 as? NSView === fading })
    }
    XCTAssertFalse(fading.accessibilityParent() is NSAccessibilityElement)
    // A point over it finds the item that slid in under it.
    let hit = list.accessibilityHitTest(NSPoint(x: frame.midX, y: frame.midY))
    XCTAssertFalse(hit as? NSView === fading)
    XCTAssertTrue(hit as? ShadowListKitListCell === list.cellForItem(at: 2))
    spin()
    XCTAssertEqual(names(posted()), [.rowCountChanged])
  }

  func testARowKeepsItsElementUntilItsItemIsDeleted() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    autoreleasepool {
      rows(list)[3].setAccessibilityIdentifier("kept")
    }
    XCTAssertEqual(try row(list, 3).accessibilityIdentifier(), "kept")
    XCTAssertTrue(list.cellForItem(at: 3)?.accessibilityParent() as? NSAccessibilityElement === (try row(list, 3)))
    let key = harness.source.keys.remove(at: 3)
    list.deleteItems(at: IndexSet(integer: 3))
    harness.layout()
    harness.source.keys.insert(key, at: 3)
    list.insertItems(at: IndexSet(integer: 3))
    harness.layout()
    XCTAssertNil(try row(list, 3).accessibilityIdentifier())
  }

  func testAPointOverARowFindsItsCell() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let cell = try XCTUnwrap(list.cellForItem(at: 4))
    let frame = try cellFrame(list, cell)
    XCTAssertTrue(list.accessibilityHitTest(NSPoint(x: frame.midX, y: frame.midY)) as? ShadowListKitListCell === cell)
  }

  func testAPointOverTheScrollBarFindsItInAListAwayFromTheOrigin() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    list.autoresizingMask = []
    list.frame = NSRect(x: 40, y: 20, width: 280, height: 460)
    list.shadowListKit_scrollerView.flashScrollers()
    harness.layout()
    let frame = try screenRect(list, NSRect(x: list.bounds.width - 3, y: 100, width: 0, height: 0))
    let hit = list.accessibilityHitTest(frame.origin) as? NSAccessibilityElement
    XCTAssertEqual(hit?.accessibilityRole(), .scrollBar)
  }

  func testAHostHidesTheListAndItsRows() throws {
    let (harness, _) = makeHarness {
      $0.scrollDriven = true
      $0.allowsSelection = false
    }
    let list = harness.list
    let posted = record(list)
    // A driven list shows no scroll bar.
    XCTAssertEqual(list.shadowListKit_scrollerView.accessibilityChildren()?.count, 0)
    list.setAccessibilityElement(false)
    XCTAssertFalse(list.isAccessibilityElement())
    XCTAssertEqual(list.accessibilityChildren()?.count, 0)
    XCTAssertEqual(list.accessibilityRows()?.count, 0)
    XCTAssertEqual(list.accessibilityRowCount(), 0)
    harness.source.keys.append("new")
    list.insertItems(at: IndexSet(integer: 600))
    XCTAssertTrue(posted().isEmpty)
    list.setAccessibilityElement(true)
    XCTAssertEqual(list.accessibilityRowCount(), 601)
  }
}
