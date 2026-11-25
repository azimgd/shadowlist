import AppKit
import ShadowListKitAppKit
import XCTest

@MainActor
final class PersistentKeysTests: XCTestCase {
  func testRetainedCellFollowsMovesPayloadsDeletionAndReinsertion() throws {
    let source = SizedTestSource(count: 200)
    let harness = ListHarness(source: source) { $0.persistentKeys = ["item-150", "later"] }
    let list = harness.list
    let cell = try XCTUnwrap(list.cellForItem(at: 150) as? TestCell)
    XCTAssertFalse(NSLocationInRange(150, list.visibleRange))
    source.keys.insert(source.keys.remove(at: 150), at: 155)
    list.reloadData()
    harness.layout()
    XCTAssertTrue(list.cellForItem(at: 155) === cell)
    list.reloadItems(at: IndexSet(integer: 155), payload: "changed")
    harness.layout()
    XCTAssertTrue(list.cellForItem(at: 155) === cell)
    XCTAssertEqual(cell.reconfigured, 1)
    source.keys.remove(at: 155)
    list.reloadData()
    harness.layout()
    XCTAssertNil(list.cellForItem(at: 155))
    XCTAssertTrue(list.persistentKeys!.contains("item-150"))
    source.keys.insert("item-150", at: 170)
    list.reloadData()
    harness.layout()
    XCTAssertNotNil(list.cellForItem(at: 170))
    list.persistentKeys = []
    harness.layout()
    XCTAssertNil(list.cellForItem(at: 170))
  }

  func testRetainedRowsMeasureAtTheCurrentCrossSize() {
    let source = SizedTestSource(count: 200)
    let harness = ListHarness(source: source) { $0.persistentKeys = ["item-150"] }
    XCTAssertEqual(harness.list.cellForItem(at: 150)?.bounds.width, 320)
    harness.list.frame.size.width = 480
    harness.list.needsLayout = true
    harness.list.layoutSubtreeIfNeeded()
    XCTAssertEqual(harness.list.cellForItem(at: 150)?.bounds.width, 480)
  }
}
