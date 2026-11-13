import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

/*
 * The base list, the row view and the layout cache: rows by key, content versions, sizes from
 * layouts, the closures, the theme and horizontal lists.
 */
@MainActor
final class RowsTests: XCTestCase {
  override func setUp() {
    super.setUp()
    CountingRowView.fills = [:]
  }

  private func rows(_ count: Int) -> [CountingRow] {
    (0..<count).map { CountingRow(key: "row-\($0)") }
  }

  func testTheListShowsItsRowsInOrder() {
    let list = ShadowListUtilsList()
    host(list)
    list.setRows(rows(5))
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, ["row-0", "row-1", "row-2", "row-3", "row-4"])
    XCTAssertTrue(list.rowView(at: 0) is CountingRowView)
    XCTAssertEqual(list.rowView(at: 3)?.row?.key, "row-3")
    XCTAssertEqual(list.rectForItem(at: 2), CGRect(x: 0, y: 88, width: testWidth, height: 44))
  }

  func testARowWithANewContentVersionIsTheOnlyOneConfiguredAgain() {
    let list = ShadowListUtilsList()
    host(list)
    var rows = rows(4)
    list.setRows(rows)
    list.layoutIfNeeded()
    XCTAssertEqual(CountingRowView.fills, ["row-0": 1, "row-1": 1, "row-2": 1, "row-3": 1])
    rows[2].contentVersion = 7
    rows[2].height = 60
    list.setRows(rows)
    list.layoutIfNeeded()
    XCTAssertEqual(CountingRowView.fills, ["row-0": 1, "row-1": 1, "row-2": 2, "row-3": 1])
    XCTAssertEqual(list.rectForItem(at: 2).height, 60)
    XCTAssertEqual(list.rectForItem(at: 3).minY, 148)
  }

  func testADeletedRowLeavesAndAnInsertedRowArrives() {
    let list = ShadowListUtilsList()
    host(list)
    var rows = rows(4)
    list.setRows(rows)
    list.layoutIfNeeded()
    rows.remove(at: 1)
    rows.append(CountingRow(key: "row-9"))
    list.setRows(rows, animated: true)
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, ["row-0", "row-2", "row-3", "row-9"])
    XCTAssertEqual(list.rowView(at: 1)?.row?.key, "row-2")
    XCTAssertEqual(CountingRowView.fills["row-9"], 1)
    XCTAssertFalse(list.animatesChanges)
  }

  func testTheClosuresRunForTheKitsDelegateCalls() {
    let list = ShadowListUtilsList()
    host(list)
    list.setRows(rows(3))
    var calls: [String] = []
    list.onEndReached = { calls.append("end") }
    list.onStartReached = { calls.append("start") }
    list.onScroll = { calls.append("scroll") }
    list.onRefresh = { calls.append("refresh") }
    list.delegate?.listViewDidReachEnd?(list)
    list.delegate?.listViewDidReachStart?(list)
    list.delegate?.listViewDidScroll?(list)
    list.delegate?.listViewDidBeginRefreshing?(list)
    XCTAssertEqual(calls, ["end", "start", "scroll", "refresh"])
  }

  func testOnRefreshTurnsPullToRefreshOnAndOff() {
    let list = ShadowListUtilsList()
    XCTAssertFalse(list.refreshEnabled)
    list.onRefresh = {}
    XCTAssertTrue(list.refreshEnabled)
    list.onRefresh = nil
    XCTAssertFalse(list.refreshEnabled)
  }

  func testTheListIsItsOwnDataSourceAndDelegate() {
    let list = ShadowListUtilsList()
    XCTAssertNotNil(list.dataSource)
    XCTAssertTrue(list.dataSource === list.delegate)
    XCTAssertEqual(list.dataSource?.numberOfItems(in: list), 0)
  }

  func testTheThemeSetsTheBackgroundAndConfiguresEveryCellAgain() {
    let list = ShadowListUtilsList(theme: .light)
    host(list)
    list.setRows(rows(3))
    list.layoutIfNeeded()
    XCTAssertEqual(list.backgroundColor, ShadowListUtilsTheme.light.colors.background)
    list.theme = .dark
    list.layoutIfNeeded()
    XCTAssertEqual(list.backgroundColor, ShadowListUtilsTheme.dark.colors.background)
    XCTAssertEqual(CountingRowView.fills, ["row-0": 2, "row-1": 2, "row-2": 2])
  }

  func testANewTextSizeLaysEveryRowOutAgain() {
    struct TextRow: ShadowListUtilsRow {
      let key: String
      var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }
      func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
        ShadowListUtilsRowLayout(width: width, height: ShadowListUtilsTheme.light.typography.body.layout("One line", width: width).size.height)
      }
    }
    let list = ShadowListUtilsList()
    host(list)
    list.setRows([TextRow(key: "a"), TextRow(key: "b")])
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 1), CGRect(x: 0, y: 22, width: testWidth, height: 22))
    withTextSize(2) {
      NotificationCenter.default.post(name: UIContentSizeCategory.didChangeNotification, object: nil)
      list.layoutIfNeeded()
      XCTAssertEqual(list.rectForItem(at: 1), CGRect(x: 0, y: 44, width: testWidth, height: 44))
      XCTAssertEqual(CountingRowView.fills, ["a": 2, "b": 2])
    }
  }

  func testANarrowerListLaysItsRowsOutForTheNewWidth() {
    struct WidthRow: ShadowListUtilsRow {
      let key: String
      var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }
      func layout(width: CGFloat) -> ShadowListUtilsRowLayout { ShadowListUtilsRowLayout(width: width, height: width / 10) }
    }
    let list = ShadowListUtilsList()
    host(list)
    list.setRows([WidthRow(key: "a"), WidthRow(key: "b")])
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 1), CGRect(x: 0, y: 39, width: 390, height: 39))
    list.frame.size.width = 300
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 1), CGRect(x: 0, y: 30, width: 300, height: 30))
  }

  func testAHorizontalListReadsALayoutsWidthAsTheSizeAlongItsAxis() {
    struct WideRow: ShadowListUtilsRow {
      let key: String
      var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }
      func layout(width: CGFloat) -> ShadowListUtilsRowLayout { ShadowListUtilsRowLayout(width: 120, height: width) }
    }
    let list = ShadowListUtilsList()
    list.isHorizontal = true
    host(list, size: CGSize(width: testWidth, height: 200))
    list.setRows((0..<4).map { WideRow(key: "wide-\($0)") })
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 2), CGRect(x: 240, y: 0, width: 120, height: 200))
    XCTAssertEqual(list.rowView(at: 1)?.frame.size, CGSize(width: 120, height: 200))
  }

  func testAHorizontalRowViewKeepsTheLayoutOfItsCrossSize() {
    final class Counter {
      var layouts = 0
    }
    struct ShortRow: ShadowListUtilsRow {
      let key: String
      let counter: Counter
      var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }
      // The content is shorter than the list is tall.
      func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
        counter.layouts += 1
        return ShadowListUtilsRowLayout(width: 120, height: width / 2)
      }
    }
    let counter = Counter()
    let list = ShadowListUtilsList()
    list.isHorizontal = true
    host(list, size: CGSize(width: testWidth, height: 200))
    list.setRows((0..<2).map { ShortRow(key: "short-\($0)", counter: counter) })
    list.layoutIfNeeded()
    let laidOut = counter.layouts
    XCTAssertEqual(laidOut, 2)
    list.rowView(at: 0)?.setNeedsLayout()
    list.rowView(at: 0)?.layoutIfNeeded()
    XCTAssertEqual(counter.layouts, laidOut)
    XCTAssertEqual(list.rowView(at: 1)?.frame, CGRect(x: 120, y: 0, width: 120, height: 200))
  }

  func testARowViewWithoutALayoutSizesItselfFromItsRow() {
    let view = CountingRowView(reuseIdentifier: nil)
    view.configure(CountingRow(key: "self-sized", height: 72), layout: nil)
    XCTAssertEqual(view.sizeThatFits(CGSize(width: 320, height: CGFloat.greatestFiniteMagnitude)), CGSize(width: 320, height: 72))
    view.prepareForReuse()
    XCTAssertNil(view.row)
  }

  func testAMountedCellTakesANewRowWithTheSameContentVersion() {
    struct TaggedRow: ShadowListUtilsRow {
      let key: String
      let tag: Int
      var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }
      func layout(width: CGFloat) -> ShadowListUtilsRowLayout { ShadowListUtilsRowLayout(width: width, height: 44) }
    }
    let list = ShadowListUtilsList()
    host(list)
    list.setRows([TaggedRow(key: "a", tag: 1)])
    list.layoutIfNeeded()
    list.setRows([TaggedRow(key: "a", tag: 2)])
    XCTAssertEqual((list.rowView(at: 0)?.row as? TaggedRow)?.tag, 2)
    XCTAssertEqual(CountingRowView.fills, ["a": 1])
  }

  func testAnAppendInsertsItsRowsAndKeepsTheOthers() {
    let list = ShadowListUtilsList()
    host(list)
    var rows = rows(3)
    list.setRows(rows)
    list.layoutIfNeeded()
    let first = list.rowView(at: 0)
    rows.append(CountingRow(key: "row-3"))
    list.setRows(rows)
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 3), CGRect(x: 0, y: 132, width: testWidth, height: 44))
    XCTAssertEqual(list.rowView(at: 3)?.row?.key, "row-3")
    rows.insert(CountingRow(key: "row-9"), at: 0)
    list.setRows(rows)
    list.layoutIfNeeded()
    XCTAssertEqual(list.rowView(at: 0)?.row?.key, "row-9")
    XCTAssertTrue(list.rowView(at: 1) === first)
    XCTAssertEqual(CountingRowView.fills, ["row-0": 1, "row-1": 1, "row-2": 1, "row-3": 1, "row-9": 1])
    // A change next to the new rows is no pure append and goes through the diff.
    rows[1].contentVersion = 5
    rows.append(CountingRow(key: "row-4"))
    list.setRows(rows)
    list.layoutIfNeeded()
    XCTAssertEqual(CountingRowView.fills["row-0"], 2)
    XCTAssertEqual(list.keys.count, 6)
  }

  func testTheCacheKeepsALayoutPerKeyWidthAndContentVersion() {
    let cache = ShadowListUtilsLayoutCache()
    var row = CountingRow(key: "cached")
    let first = cache.layout(row, width: 300)
    XCTAssertTrue(cache.layout(row, width: 300) === first)
    XCTAssertFalse(cache.layout(row, width: 200) === first)
    let narrow = cache.layout(row, width: 200)
    row.contentVersion = 1
    XCTAssertFalse(cache.layout(row, width: 200) === narrow)
    let current = cache.layout(row, width: 200)
    cache.invalidate("cached")
    XCTAssertFalse(cache.layout(row, width: 200) === current)
    let last = cache.layout(row, width: 200)
    cache.invalidateAll()
    XCTAssertFalse(cache.layout(row, width: 200) === last)
  }

  func testTheCacheDropsTheLayoutsOfKeysThatLeftTheRows() {
    let list = ShadowListUtilsList()
    host(list)
    list.setRows(rows(6))
    list.layoutIfNeeded()
    XCTAssertEqual(list.layoutCache.count, 6)
    list.setRows(Array(rows(6).suffix(2)))
    list.layoutIfNeeded()
    XCTAssertEqual(list.layoutCache.count, 2)
  }

  func testALayoutMadeWhileLayoutsWereDroppedIsNotKept() {
    // A row whose layout drops the cache's layouts, as a text size change does while a prefetch is in flight.
    struct DroppingRow: ShadowListUtilsRow {
      let key: String
      let cache: ShadowListUtilsLayoutCache
      var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }
      func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
        cache.invalidateAll()
        return ShadowListUtilsRowLayout(width: width, height: 10)
      }
    }
    let cache = ShadowListUtilsLayoutCache()
    let stale = cache.layout(DroppingRow(key: "dropped", cache: cache), width: 300)
    XCTAssertEqual(cache.count, 0)
    let kept = cache.layout(CountingRow(key: "dropped"), width: 300)
    XCTAssertFalse(kept === stale)
    XCTAssertEqual(cache.count, 1)
    XCTAssertTrue(cache.layout(CountingRow(key: "dropped"), width: 300) === kept)
  }

  func testPrefetchLaysRowsOutAheadOnItsQueue() {
    final class Counter: @unchecked Sendable {
      private let lock = NSLock()
      private var count = 0
      func add() { lock.lock(); count += 1; lock.unlock() }
      var value: Int { lock.lock(); defer { lock.unlock() }; return count }
    }
    struct CountedRow: ShadowListUtilsRow {
      let key: String
      let counter: Counter
      var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }
      func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
        counter.add()
        return ShadowListUtilsRowLayout(width: width, height: 10)
      }
    }
    let counter = Counter()
    let cache = ShadowListUtilsLayoutCache()
    let rows = (0..<20).map { CountedRow(key: "row-\($0)", counter: counter) }
    cache.prefetch(rows, width: 300, around: 10)
    let done = expectation(description: "prefetched")
    func poll() {
      if counter.value == rows.count {
        done.fulfill()
      } else {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.01, execute: poll)
      }
    }
    poll()
    wait(for: [done], timeout: 5)
    _ = cache.layout(rows[3], width: 300)
    XCTAssertEqual(counter.value, rows.count)
  }

  func testAMovedItemLandsAtItsDestination() {
    XCTAssertEqual(ShadowListUtilsArrayMove.moved(["a", "b", "c", "d"], from: 0, to: 2), ["b", "c", "a", "d"])
    XCTAssertEqual(ShadowListUtilsArrayMove.moved(["a", "b", "c", "d"], from: 3, to: 1), ["a", "d", "b", "c"])
    XCTAssertEqual(ShadowListUtilsArrayMove.moved(["a", "b"], from: 1, to: 1), ["a", "b"])
    XCTAssertEqual(ShadowListUtilsArrayMove.moved(["a", "b"], from: 0, to: 5), ["a", "b"])
  }
}
