import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Mounting, recycling, geometry, sizing and prefetching of a plain list.
 */
@MainActor
final class ListBasicsTests: XCTestCase {
  func testMountsTheCellsOfTheViewportFromTheTop() throws {
    let harness = ListHarness(source: TestSource(count: 100))
    let list = harness.list
    XCTAssertEqual(list.contentSize.height, 100 * 44, accuracy: 0.5)
    XCTAssertEqual(list.visibleRange.location, 0)
    XCTAssertEqual(list.visibleRange.length, 11)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 0)), 0, accuracy: 0.01)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 3)), 132, accuracy: 0.01)
    let cell = try XCTUnwrap(list.cellForItem(at: 3) as? TestCell)
    XCTAssertEqual(cell.key, "item-3")
    XCTAssertEqual(cell.index, 3)
    XCTAssertEqual(cell.frame.size, NSSize(width: 320, height: 44))
    XCTAssertTrue(cell.isFlipped)
    XCTAssertTrue(list.isFlipped)
    XCTAssertEqual(list.visibleCells.count, 11)
    XCTAssertTrue(harness.delegate.displayed.contains(0))
    XCTAssertGreaterThan(harness.delegate.layOuts, 0)
  }

  func testScrollingRecyclesCellsThatLeaveAndReusesThem() throws {
    let source = TestSource(count: 500)
    let harness = ListHarness(source: source)
    let list = harness.list
    let made = source.cellsMade
    for step in 1...40 {
      list.contentOffset = CGPoint(x: 0, y: CGFloat(step) * 200)
      harness.layout()
    }
    XCTAssertEqual(list.visibleRange.location, 8000 / 44)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 182)), 182 * 44 - 8000, accuracy: 0.5)
    XCTAssertTrue(harness.delegate.ended.contains(0))
    // Cells come back from the pool. Only the first screens and the overscan were made new.
    let fresh = list.subviews.first!.subviews.count
    XCTAssertLessThan(fresh, 60)
    XCTAssertGreaterThan(source.cellsMade - made, 100)
    XCTAssertNil(list.cellForItem(at: 0))
  }

  func testWheelScrollsWithinTheContent() throws {
    let harness = ListHarness(source: SizedTestSource(count: 40))
    let list = harness.list
    func wheel(_ delta: Int32) throws {
      let cgEvent = try XCTUnwrap(CGEvent(scrollWheelEvent2Source: nil, units: .pixel, wheelCount: 1, wheel1: delta, wheel2: 0, wheel3: 0))
      list.scrollWheel(with: try XCTUnwrap(NSEvent(cgEvent: cgEvent)))
      harness.layout()
    }
    try wheel(-40)
    XCTAssertEqual(list.contentOffset.y, 40, accuracy: 0.01)
    XCTAssertEqual(harness.delegate.scrolls, 1)
    try wheel(200)
    XCTAssertEqual(list.contentOffset.y, 0, accuracy: 0.01)
    for _ in 0..<60 {
      try wheel(-100)
    }
    XCTAssertEqual(list.contentOffset.y, 40 * 44 - 480, accuracy: 0.01)
    XCTAssertNotNil(list.cellForItem(at: 39))
  }

  func testRectForItemIsInContentCoordinates() {
    let source = SizedTestSource(count: 50)
    source.heights["item-2"] = 100
    let harness = ListHarness(source: source)
    let list = harness.list
    XCTAssertEqual(list.rectForItem(at: 3), CGRect(x: 0, y: 188, width: 320, height: 44))
    list.contentOffset = CGPoint(x: 0, y: 300)
    harness.layout()
    XCTAssertEqual(list.rectForItem(at: 3), CGRect(x: 0, y: 188, width: 320, height: 44))
    XCTAssertTrue(list.rectForItem(at: 500).isNull)
  }

  func testSizesComeFromTheDataSourceWithoutMeasuringCells() {
    let source = SizedTestSource(count: 200)
    source.defaultHeight = 30
    let harness = ListHarness(source: source)
    XCTAssertGreaterThan(source.sizeCalls, 0)
    XCTAssertEqual(harness.list.visibleRange.length, 16)
    XCTAssertEqual(harness.list.rectForItem(at: 10).minY, 300, accuracy: 0.01)
  }

  func testSizesComeFromSizeThatFitsWithoutASizeFromTheDataSource() {
    let source = TestSource(count: 20)
    source.heights["item-1"] = 90
    let harness = ListHarness(source: source)
    XCTAssertEqual(harness.list.rectForItem(at: 1).height, 90, accuracy: 0.01)
    XCTAssertEqual(harness.list.rectForItem(at: 2).minY, 134, accuracy: 0.01)
  }

  func testDefaultSizeThatFitsFollowsAutoLayout() {
    let cell = ShadowListKitListCell(reuseIdentifier: nil)
    let label = NSTextField(wrappingLabelWithString: String(repeating: "Wrapped words in a row. ", count: 12))
    label.translatesAutoresizingMaskIntoConstraints = false
    cell.addSubview(label)
    NSLayoutConstraint.activate([
      label.leadingAnchor.constraint(equalTo: cell.leadingAnchor, constant: 8),
      label.trailingAnchor.constraint(equalTo: cell.trailingAnchor, constant: -8),
      label.topAnchor.constraint(equalTo: cell.topAnchor, constant: 10),
      label.bottomAnchor.constraint(equalTo: cell.bottomAnchor, constant: -10),
    ])
    let narrow = cell.sizeThatFits(NSSize(width: 160, height: CGFloat.greatestFiniteMagnitude))
    let wide = cell.sizeThatFits(NSSize(width: 480, height: CGFloat.greatestFiniteMagnitude))
    XCTAssertEqual(narrow.width, 160)
    XCTAssertGreaterThan(narrow.height, wide.height)
    XCTAssertGreaterThan(wide.height, 20)
    XCTAssertEqual(cell.constraints.count, 4)
  }

  func testPrefetchReportsItemsAheadOfTheMountedOnes() {
    final class Prefetch: NSObject, ShadowListKitListViewPrefetchDataSource {
      var prefetched = IndexSet()
      var cancelled = IndexSet()
      func listView(_ listView: ShadowListKitListView, prefetchItemsAt indices: IndexSet) { prefetched.formUnion(indices) }
      func listView(_ listView: ShadowListKitListView, cancelPrefetchingForItemsAt indices: IndexSet) { cancelled.formUnion(indices) }
    }
    let prefetch = Prefetch()
    let harness = ListHarness(source: SizedTestSource(count: 300)) { $0.prefetchDataSource = prefetch }
    let list = harness.list
    for step in 1...10 {
      list.contentOffset = CGPoint(x: 0, y: CGFloat(step) * 100)
      harness.layout()
    }
    // Items past the mounted cells and the viewport come first, before they show.
    XCTAssertFalse(prefetch.prefetched.isEmpty)
    XCTAssertGreaterThan(prefetch.prefetched.max() ?? 0, list.visibleRange.upperBound + 4)
    list.contentOffset = CGPoint(x: 0, y: 9000)
    harness.layout()
    XCTAssertFalse(prefetch.cancelled.isEmpty)
  }

  func testSectionTitlesShowInTitleCellsBetweenItems() throws {
    final class Sections: TestSource {
      func numberOfSections(in listView: ShadowListKitListView) -> Int { 3 }
      func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { 4 }
      func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { "Section \(section)" }
    }
    let harness = ListHarness(source: Sections(count: 12))
    let list = harness.list
    XCTAssertEqual(list.numberOfSections, 3)
    XCTAssertEqual(list.sectionForItem(at: 5), 1)
    XCTAssertEqual(list.firstItemIndexInSection(2), 8)
    let header = list.rectForHeader(inSection: 1)
    XCTAssertEqual(header.minY, list.rectForItem(at: 3).maxY, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 4).minY, header.maxY, accuracy: 0.01)
    let titles = list.subviews.first!.subviews.compactMap { view -> String? in
      guard !view.isHidden else { return nil }
      return view.subviews.compactMap { ($0 as? NSTextField)?.stringValue }.first
    }
    XCTAssertTrue(titles.contains("Section 1"))
  }
}
