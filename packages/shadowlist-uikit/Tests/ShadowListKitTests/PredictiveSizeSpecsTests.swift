import ShadowListKit
import UIKit
import XCTest

@MainActor
final class PredictiveSizeSpecsTests: XCTestCase {
  private final class Source: NSObject, ShadowListKitListViewDataSource {
    var keys = (0..<200).map { "spec-\($0)" }
    func numberOfItems(in listView: ShadowListKitListView) -> Int { keys.count }
    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }
    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "cell")
    }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 40 }
  }

  private func drain(_ list: ShadowListKitListView) {
    for _ in 0..<8 { list.setNeedsLayout(); list.layoutIfNeeded() }
  }
  private func makeList() -> (ShadowListKitListView, Source) {
    let list = ShadowListKitListView(frame: CGRect(x: 0, y: 0, width: 320, height: 400))
    let source = Source()
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "cell")
    list.dataSource = source
    list.reloadData()
    return (list, source)
  }

  func testPredictionsCacheByKeyAndDisablingKeepsExactMeasurements() {
    let (list, source) = makeList()
    var calls: [Int: Int] = [:]
    XCTAssertEqual(list.measureLookaheadRows, 48)
    list.getItemSizeSpec = { index in
      XCTAssertTrue(Thread.isMainThread)
      calls[index, default: 0] += 1
      let spec = ShadowListKitItemSizeSpec(text: "prediction")
      spec.fixedHeight = 180
      return spec
    }
    drain(list)
    XCTAssertEqual(list.rectForItem(at: 0).height, 40)
    XCTAssertEqual(list.rectForItem(at: 40).height, 180)
    XCTAssertEqual(calls[40], 1)
    drain(list)
    XCTAssertEqual(calls[40], 1)
    list.getItemSizeSpec = nil
    drain(list)
    XCTAssertEqual(list.rectForItem(at: 0).height, 40)
    XCTAssertEqual(list.rectForItem(at: 40).height, 40)
    withExtendedLifetime(source) {}
  }

  func testTextSpecRewrapsAndFontConfigurationRequeriesTheProvider() {
    let (list, source) = makeList()
    var lines = 0
    var calls = 0
    list.getItemSizeSpec = { _ in
      calls += 1
      let spec = ShadowListKitItemSizeSpec(text: String(repeating: "word ", count: 30))
      spec.fontSize = 14
      spec.lineHeight = 20
      spec.insetWidth = 30
      spec.insetHeight = 12
      spec.widthFraction = 0.75
      spec.numberOfLines = lines
      return spec
    }
    drain(list)
    let narrow = list.rectForItem(at: 40).height
    list.frame.size.width = 640
    drain(list)
    XCTAssertLessThan(list.rectForItem(at: 40).height, narrow)
    lines = 1
    let before = calls
    list.traitCollectionDidChange(UITraitCollection(preferredContentSizeCategory: .extraLarge))
    drain(list)
    XCTAssertGreaterThan(calls, before)
    XCTAssertEqual(list.rectForItem(at: 40).height, 32, accuracy: 0.01)
    XCTAssertEqual(list.rectForItem(at: 0).height, 40)
    withExtendedLifetime(source) {}
  }

  func testHorizontalFixedHeightRemainsHeightAndRetainedKeysDoNotDuplicateCallbacks() {
    let (list, source) = makeList()
    list.isHorizontal = true
    list.persistentKeys = ["spec-150"]
    var calls: [Int: Int] = [:]
    list.getItemSizeSpec = { index in
      calls[index, default: 0] += 1
      let spec = ShadowListKitItemSizeSpec(text: "")
      spec.fixedHeight = 180
      return index == 41 ? nil : spec
    }
    drain(list)
    XCTAssertEqual(calls[150], 1)
    XCTAssertEqual(list.rectForItem(at: 40).width, 320)
    // ListDriver.getRowRect stretches a single horizontal row to windowCross. fixedHeight
    // stays a literal core prediction, not a rendered cross-size override or width alias.
    XCTAssertEqual(list.rectForItem(at: 40).height, 400)
    XCTAssertEqual(list.rectForItem(at: 41).width, 40)
    withExtendedLifetime(source) {}
  }
}
