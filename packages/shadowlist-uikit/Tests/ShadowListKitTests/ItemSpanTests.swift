import ShadowListKit
import UIKit
import XCTest

@MainActor
final class ItemSpanTests: XCTestCase {
  private class Source: NSObject, ShadowListKitListViewDataSource {
    var keys = (0 ..< 200).map { "span-\($0)" }
    var crosses: [Int: CGFloat] = [:]
    func numberOfItems(in listView: ShadowListKitListView) -> Int {
      keys.count
    }

    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
      keys[index]
    }

    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "cell")
    }

    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
      crosses[index] = crossSize
      return 40
    }
  }

  private final class Sections: Source {
    override init() {
      super.init(); keys = (0 ..< 4).map { "section-item-\($0)" }
    }

    func numberOfSections(in listView: ShadowListKitListView) -> Int {
      3
    }

    func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int {
      section == 1 ? 0 : 2
    }

    func listView(
      _ listView: ShadowListKitListView,
      titleForHeaderInSection section: Int
    ) -> String? {
      "Header \(section)"
    }

    func listView(
      _ listView: ShadowListKitListView,
      titleForFooterInSection section: Int
    ) -> String? {
      section == 0 ? "Footer" : nil
    }

    func listView(
      _ listView: ShadowListKitListView,
      sizeForHeaderInSection section: Int,
      crossSize: CGFloat
    ) -> CGFloat {
      section == 1 ? 0 : 10
    }

    func listView(
      _ listView: ShadowListKitListView,
      sizeForFooterInSection section: Int,
      crossSize: CGFloat
    ) -> CGFloat {
      0
    }
  }

  private func drain(_ list: ShadowListKitListView) {
    for _ in 0 ..< 6 {
      list.setNeedsLayout(); list.layoutIfNeeded()
    }
  }

  func testSpansReachOffscreenRowsAndUseTheSameWidthForMeasurement() {
    let source = Source()
    let list = ShadowListKitListView(frame: CGRect(x: 0, y: 0, width: 360, height: 400))
    list.numberOfColumns = 3
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "cell")
    list.dataSource = source
    var calls = 0
    list.getItemSpan = { index in
      XCTAssertTrue(Thread.isMainThread)
      calls += 1
      return index % 3 + 1
    }
    list.reloadData()
    drain(list)
    XCTAssertEqual(calls, 200)
    XCTAssertEqual(list.rectForItem(at: 0).width, 120, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 1).width, 240, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 2).width, 360, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 2).minY, 40, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 197).width, 360, accuracy: 0.01)
    XCTAssertEqual(source.crosses[1], 240)
    drain(list)
    XCTAssertEqual(calls, 200)
    withExtendedLifetime(source) {}
  }

  func testColumnChangesAndIndexDependentSpansRequeryTheCurrentSource() {
    let source = Source()
    let list = ShadowListKitListView(frame: CGRect(x: 0, y: 0, width: 360, height: 400))
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "cell")
    list.dataSource = source
    list.numberOfColumns = 3
    list.getItemSpan = { $0 == 0 ? 99 : 2 }
    list.reloadData()
    drain(list)
    list.numberOfColumns = 2
    drain(list)
    XCTAssertEqual(list.rectForItem(at: 0).width, 360, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 1).width, 360, accuracy: 0.01)
    list.numberOfColumns = 4
    drain(list)
    XCTAssertEqual(list.rectForItem(at: 0).width, 360, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 1).width, 180, accuracy: 0.01)
    source.keys.swapAt(0, 1)
    list.reloadData()
    drain(list)
    XCTAssertEqual(list.rectForItem(at: 0).width, 360, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 1).width, 180, accuracy: 0.01)
    list.getItemSpan = nil
    drain(list)
    XCTAssertEqual(list.rectForItem(at: 0).width, 90, accuracy: 0.01)
    withExtendedLifetime(source) {}
  }

  func testSectionEdgesSpanAllTracksWithoutCallingThePublicItemProvider() {
    let source = Sections()
    let list = ShadowListKitListView(frame: CGRect(x: 0, y: 0, width: 360, height: 400))
    list.numberOfColumns = 3
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "cell")
    list.dataSource = source
    var indices: [Int] = []
    list.getItemSpan = { index in indices.append(index); return index % 2 + 1 }
    list.reloadData()
    drain(list)
    XCTAssertEqual(indices, [0, 1, 2, 3]) // Native item indices are global across sections.
    XCTAssertEqual(list.rectForHeader(inSection: 0).width, 360, accuracy: 0.01)
    XCTAssertEqual(list.rectForFooter(inSection: 0).width, 360, accuracy: 0.01)
    XCTAssertEqual(list.rectForHeader(inSection: 1).minY, 50, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 2).minY, 60, accuracy: 0.01)
    XCTAssertEqual(list.contentSize.height, 100, accuracy: 0.01)
    withExtendedLifetime(source) {}
  }
}
