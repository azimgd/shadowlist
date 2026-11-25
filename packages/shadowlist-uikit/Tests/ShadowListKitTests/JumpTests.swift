import ShadowListKit
import UIKit
import XCTest

/*
 * A jump measures and mounts the rows it shows first and the overscan one frame later. Once
 * that frame ran, the list rests where a jump into rows measured already puts it.
 */
@MainActor
final class JumpTests: XCTestCase {
  private final class Rows: NSObject, ShadowListKitListViewDataSource {
    let keys = (0 ..< 2000).map { "item-\($0)" }
    func numberOfItems(in listView: ShadowListKitListView) -> Int {
      keys.count
    }

    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
      keys[index]
    }

    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "row")
    }

    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
      CGFloat(30 + index % 7 * 13)
    }
  }

  private struct Rest: Equatable {
    let offset: CGFloat
    let contentHeight: CGFloat
    let target: CGRect
    let mounted: [Int]
  }

  private func rest(_ list: ShadowListKitListView, _ index: Int) -> Rest {
    Rest(
      offset: list.contentOffset.y, contentHeight: list.contentSize.height, target: list.rectForItem(at: index),
      mounted: (0 ..< 2000).filter { list.cellForItem(at: $0) != nil }
    )
  }

  private func frames(_ count: Int) {
    let deadline = Date(timeIntervalSinceNow: Double(count) / 30)
    RunLoop.main.run(until: deadline)
  }

  func testAJumpSettlesWhereAJumpIntoMeasuredRowsLands() {
    let rows = Rows()
    let list = ShadowListKitListView()
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.estimatedItemSize = 100
    list.dataSource = rows
    list.reloadData()
    let window = host(list, size: CGSize(width: 300, height: 600))
    defer { window.isHidden = true }

    list.scrollToItem(at: 1200, viewPosition: 0, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual(list.rectForItem(at: 1200).minY, list.contentOffset.y + list.adjustedContentInset.top, accuracy: 0.5)
    frames(6)
    let first = rest(list, 1200)
    XCTAssertGreaterThan(first.mounted.count, list.visibleCells.count)

    // The rows around 1200 are measured now. The same jump again lands the same way.
    list.scrollToItem(at: 1200, viewPosition: 0, animated: false)
    list.layoutIfNeeded()
    frames(6)
    XCTAssertEqual(rest(list, 1200), first)
  }

  func testRepeatedJumpsBeforeTheSettleFrameKeepTheLastTarget() {
    let rows = Rows()
    let list = ShadowListKitListView()
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.estimatedItemSize = 100
    list.dataSource = rows
    list.reloadData()
    let window = host(list, size: CGSize(width: 300, height: 600))
    defer { window.isHidden = true }

    list.scrollToItem(at: 1200, viewPosition: 0, animated: false)
    list.scrollToItem(at: 1200, viewPosition: 0, animated: false)
    list.scrollToItem(at: 600, viewPosition: 0, animated: false)
    frames(6)
    let settled = rest(list, 600)
    XCTAssertEqual(settled.target.minY, settled.offset + list.adjustedContentInset.top, accuracy: 0.5)
    XCTAssertGreaterThan(settled.mounted.count, list.visibleCells.count)

    list.scrollToItem(at: 600, viewPosition: 0, animated: false)
    frames(6)
    XCTAssertEqual(rest(list, 600), settled)
  }
}
