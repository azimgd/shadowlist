import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Items with a content version a test bumps by key.
 */
private final class VersionedSource: TestSource {
  var versions: [String: Int] = [:]

  func listView(_ listView: ShadowListKitListView, contentVersionForItemAt index: Int) -> Int {
    versions[keys[index]] ?? 0
  }
}

/*
 * Data changes: rows on screen stay where they are while items come and go around them.
 */
@MainActor
final class ListDataTests: XCTestCase {
  private func makeSource(count: Int, prefix: String = "item") -> TestSource {
    let source = TestSource(count: count, prefix: prefix)
    for (index, key) in source.keys.enumerated() {
      source.heights[key] = 30 + CGFloat(index % 5) * 12
    }
    return source
  }

  private func harnessInMiddle(_ source: TestSource) -> ListHarness {
    let harness = ListHarness(source: source)
    for step in 1...10 {
      harness.list.contentOffset = CGPoint(x: 0, y: CGFloat(step) * 300)
      harness.layout()
    }
    return harness
  }

  func testInsertAboveKeepsVisibleRowsStill() {
    let source = makeSource(count: 400)
    let harness = harnessInMiddle(source)
    let before = harness.visibleKeys()
    XCTAssertGreaterThan(before.count, 5)
    let added = (0..<25).map { "new-\($0)" }
    for key in added {
      source.heights[key] = 70
    }
    source.keys.insert(contentsOf: added, at: 0)
    harness.list.insertItems(at: IndexSet(0..<25))
    harness.layout()
    let (shift, kept) = harness.maxShift(from: before)
    XCTAssertLessThan(shift, 0.5)
    XCTAssertEqual(kept, before.count)
    // The rows above are placed by estimate until they show. The offset moved past them.
    XCTAssertGreaterThan(harness.list.contentOffset.y, 3000 + 25 * 30)
  }

  func testReloadDataWithPrependedKeysKeepsVisibleRowsStill() {
    let source = makeSource(count: 400)
    let harness = harnessInMiddle(source)
    let before = harness.visibleKeys()
    source.keys.insert(contentsOf: (0..<40).map { "older-\($0)" }, at: 0)
    harness.list.reloadData()
    harness.layout()
    let (shift, kept) = harness.maxShift(from: before)
    XCTAssertLessThan(shift, 0.5)
    XCTAssertEqual(kept, before.count)
  }

  func testDeleteAboveKeepsVisibleRowsStillAndDeleteOnScreenRecyclesTheCell() throws {
    let source = makeSource(count: 400)
    let harness = harnessInMiddle(source)
    let before = harness.visibleKeys()
    source.keys.removeSubrange(0..<10)
    harness.list.deleteItems(at: IndexSet(0..<10))
    harness.layout()
    XCTAssertLessThan(harness.maxShift(from: before).shift, 0.5)

    let first = harness.list.visibleRange.location + 2
    let deletedKey = source.keys[first]
    let cell = try XCTUnwrap(harness.list.cellForItem(at: first) as? TestCell)
    XCTAssertEqual(cell.key, deletedKey)
    source.keys.remove(at: first)
    harness.list.deleteItems(at: IndexSet(integer: first))
    harness.layout()
    XCTAssertTrue(cell.isHidden)
    XCTAssertTrue(harness.delegate.ended.contains(first))
    XCTAssertNil(harness.visibleKeys()[deletedKey])
    XCTAssertEqual((harness.list.cellForItem(at: first) as? TestCell)?.key, source.keys[first])
  }

  func testMoveItemReordersTheRows() throws {
    let source = makeSource(count: 50)
    let harness = ListHarness(source: source)
    let moved = source.keys.remove(at: 4)
    source.keys.insert(moved, at: 0)
    harness.list.moveItem(at: 4, to: 0)
    harness.layout()
    XCTAssertEqual((harness.list.cellForItem(at: 0) as? TestCell)?.key, "item-4")
    XCTAssertEqual((harness.list.cellForItem(at: 1) as? TestCell)?.key, "item-0")
    XCTAssertEqual(harness.list.rectForItem(at: 1).minY, harness.list.rectForItem(at: 0).maxY, accuracy: 0.01)
  }

  func testBatchUpdatesLandTogether() throws {
    let source = makeSource(count: 60)
    let harness = ListHarness(source: source)
    let reloadedCell = try XCTUnwrap(harness.list.cellForItem(at: 6) as? TestCell)
    let finished = expectation(description: "completion")
    // Before: item-0 ... item-59. Delete 1 and 3, insert two at 0, reload 6.
    harness.list.performBatchUpdates({
      source.keys.remove(at: 3)
      source.keys.remove(at: 1)
      source.keys.insert(contentsOf: ["a", "b"], at: 0)
      harness.list.deleteItems(at: IndexSet([1, 3]))
      harness.list.insertItems(at: IndexSet([0, 1]))
      harness.list.reloadItems(at: IndexSet(integer: 6), payload: "badge")
    }) { done in
      XCTAssertTrue(done)
      finished.fulfill()
    }
    harness.layout()
    wait(for: [finished], timeout: 1)
    let keys = (0..<6).compactMap { (harness.list.cellForItem(at: $0) as? TestCell)?.key }
    XCTAssertEqual(keys, ["a", "b", "item-0", "item-2", "item-4", "item-5"])
    XCTAssertEqual(reloadedCell.reconfigured, 1)
    XCTAssertEqual((harness.list.cellForItem(at: 6) as? TestCell)?.key, "item-6")
  }

  func testReloadItemsMeasuresAgain() {
    let source = makeSource(count: 30)
    let harness = ListHarness(source: source)
    source.heights["item-2"] = 140
    harness.list.reloadItems(at: IndexSet(integer: 2))
    harness.layout()
    XCTAssertEqual(harness.list.rectForItem(at: 2).height, 140, accuracy: 0.01)
    XCTAssertEqual(harness.list.rectForItem(at: 3).minY, harness.list.rectForItem(at: 2).maxY, accuracy: 0.01)
  }

  /*
   * The core leaves an inverted list past its new end. The driver brings it back and runs the core
   * again there, and the end the first run reached is still told to the delegate.
   */
  func testAnInvertedListThatShrinksToAFewRowsReachesItsEnd() {
    final class Edges: NSObject, ShadowListKitListViewDelegate {
      var ends = 0
      func listViewDidReachEnd(_ listView: ShadowListKitListView) { ends += 1 }
    }
    let edges = Edges()
    let source = makeSource(count: 400)
    let harness = ListHarness(source: source) { $0.isInverted = true }
    harness.list.contentOffset = CGPoint(x: 0, y: 3000)
    harness.layout()
    harness.list.delegate = edges
    source.keys = Array(source.keys.prefix(3))
    harness.list.reloadData()
    XCTAssertEqual(edges.ends, 1)
    XCTAssertEqual(harness.list.contentOffset.y, 0)
    harness.layout()
    XCTAssertEqual(edges.ends, 1)
  }

  /*
   * The rows the run at the new end measures are shorter than their estimates, and the content
   * shrinks once more.
   */
  func testAnInvertedListThatShrinksTwiceInOnePassRestsAtItsEnd() throws {
    let source = makeSource(count: 400)
    let harness = ListHarness(source: source) {
      $0.isInverted = true
      $0.estimatedItemSize = 200
    }
    let list = harness.list
    list.contentOffset = CGPoint(x: 0, y: 3000)
    harness.layout()
    source.keys = Array(source.keys.prefix(13))
    list.deleteItems(at: IndexSet(13..<400))
    XCTAssertEqual(list.contentOffset.y, list.contentSize.height - 480, accuracy: 0.01)
    harness.layout()
    XCTAssertEqual(list.contentSize.height, 666, accuracy: 0.01)
    XCTAssertEqual(list.contentOffset.y, 186, accuracy: 0.01)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 12)), 480 - 54, accuracy: 0.01)
  }

  func testApplyChangesFindsInsertsDeletesAndMoves() {
    let source = makeSource(count: 400)
    let harness = harnessInMiddle(source)
    let before = harness.visibleKeys()
    source.keys.remove(at: 5)
    source.keys.insert("fresh", at: 0)
    let moved = source.keys.remove(at: 20)
    source.keys.insert(moved, at: 2)
    let changes = harness.list.applyChanges()
    harness.layout()
    XCTAssertEqual(changes.deletedIndices, IndexSet(integer: 5))
    XCTAssertEqual(changes.insertedIndices, IndexSet(integer: 0))
    XCTAssertFalse(changes.movedFromIndices.isEmpty)
    XCTAssertLessThan(harness.maxShift(from: before).shift, 0.5)
  }

  func testAnItemInsertedThroughItsOwnCallIsReloadedOnceItsContentChanges() {
    let source = VersionedSource(count: 10)
    let harness = ListHarness(source: source)
    source.keys.insert("fresh", at: 2)
    harness.list.insertItems(at: IndexSet(integer: 2))
    harness.layout()
    source.versions["fresh"] = 1
    XCTAssertEqual(harness.list.applyChanges().reloadedIndices, IndexSet(integer: 2))
    XCTAssertTrue(harness.list.applyChanges().reloadedIndices.isEmpty)
  }

  func testAnInsertKeepsTheChangeOfAnotherItemForTheNextDiff() {
    let source = VersionedSource(count: 10)
    let harness = ListHarness(source: source)
    // Item 5 changed and nobody told the list yet.
    source.versions[source.keys[5]] = 1
    source.keys.insert("fresh", at: 0)
    harness.list.insertItems(at: IndexSet(integer: 0))
    harness.layout()
    XCTAssertEqual(harness.list.applyChanges().reloadedIndices, IndexSet(integer: 6))
  }

  func testAnItemInsertedInABatchIsReloadedOnceItsContentChanges() {
    let source = VersionedSource(count: 10)
    let harness = ListHarness(source: source)
    source.versions[source.keys[7]] = 1
    harness.list.performBatchUpdates({
      source.keys.remove(at: 0)
      harness.list.deleteItems(at: IndexSet(integer: 0))
      source.keys.insert("fresh", at: 3)
      harness.list.insertItems(at: IndexSet(integer: 3))
    }, completion: nil)
    harness.layout()
    source.versions["fresh"] = 1
    XCTAssertEqual(harness.list.applyChanges().reloadedIndices, IndexSet([3, 7]))
  }
}
