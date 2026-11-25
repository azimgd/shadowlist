import ShadowListKit
import UIKit
import XCTest

@MainActor
final class PersistentKeysTests: XCTestCase {
  private final class Source: NSObject, ShadowListKitListViewDataSource {
    var keys = (0 ..< 200).map { "row-\($0)" }
    var reconfigured = 0
    func numberOfItems(in listView: ShadowListKitListView) -> Int {
      keys.count
    }

    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
      keys[index]
    }

    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
      40
    }

    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "cell")
    }

    func listView(
      _ listView: ShadowListKitListView,
      reconfigureCell cell: ShadowListKitListCell,
      at index: Int,
      payload: Any?
    ) -> Bool {
      reconfigured += 1
      cell.accessibilityLabel = payload as? String
      return true
    }
  }

  private func makeList() -> (ShadowListKitListView, Source) {
    let list = ShadowListKitListView(frame: CGRect(x: 0, y: 0, width: 320, height: 400))
    let source = Source()
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "cell")
    list.dataSource = source
    list.persistentKeys = ["row-150", "later"]
    list.reloadData()
    list.layoutIfNeeded()
    return (list, source)
  }

  func testOffscreenPayloadMoveDeletionReinsertionAndClearing() throws {
    let (list, source) = makeList()
    let cell = try XCTUnwrap(list.cellForItem(at: 150))
    XCTAssertFalse(NSLocationInRange(150, list.visibleRange))
    source.keys.insert(source.keys.remove(at: 150), at: 155)
    list.reloadData()
    list.layoutIfNeeded()
    XCTAssertTrue(list.cellForItem(at: 155) === cell)
    list.reloadItems(at: IndexSet(integer: 155), payload: "changed")
    list.layoutIfNeeded()
    XCTAssertTrue(list.cellForItem(at: 155) === cell)
    XCTAssertEqual(cell.accessibilityLabel, "changed")
    XCTAssertEqual(source.reconfigured, 1)
    source.keys.remove(at: 155)
    list.reloadData()
    list.layoutIfNeeded()
    XCTAssertNil(list.cellForItem(at: 155))
    XCTAssertTrue(list.persistentKeys!.contains("row-150"))
    source.keys.insert("row-150", at: 170)
    list.reloadData()
    list.layoutIfNeeded()
    XCTAssertNotNil(list.cellForItem(at: 170))
    list.persistentKeys = []
    list.layoutIfNeeded()
    XCTAssertNil(list.cellForItem(at: 170))
  }

  func testRetainedMeasurementFollowsViewportWidthAndOrientation() {
    let (list, source) = makeList()
    withExtendedLifetime(source) {
      XCTAssertEqual(list.cellForItem(at: 150)?.bounds.width, 320)
      list.frame.size.width = 480
      list.setNeedsLayout()
      list.layoutIfNeeded()
      XCTAssertEqual(list.cellForItem(at: 150)?.bounds.width, 480)
      list.isHorizontal = true
      list.setNeedsLayout()
      list.layoutIfNeeded()
      XCTAssertEqual(list.cellForItem(at: 150)?.bounds.width, 40)
      XCTAssertEqual(list.cellForItem(at: 150)?.bounds.height, 400)
    }
  }
}
