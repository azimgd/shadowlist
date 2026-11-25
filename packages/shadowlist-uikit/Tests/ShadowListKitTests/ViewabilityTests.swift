import ShadowListKit
import UIKit
import XCTest

@MainActor
final class ViewabilityTests: XCTestCase {
  private final class List: ShadowListKitListView {
    var draggingPhase = false
    override var isDragging: Bool { draggingPhase }
  }

  private class Source: NSObject, ShadowListKitListViewDataSource {
    var heights = Array(repeating: CGFloat(100), count: 20)
    var keys = (0..<20).map { "public-\($0)" }
    func numberOfItems(in listView: ShadowListKitListView) -> Int { heights.count }
    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }
    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "row")
    }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { heights[index] }
  }

  private class SectionSource: Source {
    var firstCount = 2
    override init() { super.init(); heights = [40, 40, 40, 40] }
    func numberOfSections(in listView: ShadowListKitListView) -> Int { 2 }
    func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { section == 0 ? firstCount : 2 }
    func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { "Section \(section)" }
    func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat { 20 }
  }

  private final class StableSectionSource: SectionSource {
    func listView(_ listView: ShadowListKitListView, keyForSection section: Int) -> String { "section-\(section)" }
  }

  private func config(_ percent: Double = 50, time: Double = 0, wait: Bool = false) -> ShadowListKitViewabilityConfig {
    ShadowListKitViewabilityConfig(itemVisiblePercentThreshold: NSNumber(value: percent),
      viewAreaCoveragePercentThreshold: nil, minimumViewTime: time, waitForInteraction: wait)
  }
  private func fixture(_ source: Source, automaticInsets: Bool = false) -> (ShadowListKitListView, UIWindow) {
    let list = List()
    // These tests specify the viewport in content coordinates, without device safe-area insets.
    list.contentInsetAdjustmentBehavior = automaticInsets ? .automatic : .never
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.dataSource = source
    list.reloadData()
    let window = host(list, size: CGSize(width: 200, height: 100))
    return (list, window)
  }
  private func pump(_ seconds: TimeInterval) {
    RunLoop.main.run(until: Date(timeIntervalSinceNow: seconds))
  }
  private func layout(_ list: ShadowListKitListView) {
    list.setNeedsLayout()
    list.layoutIfNeeded()
  }

  func testPairsOverrideSingleAndEqualConfigCallbackReplacementDoesNotResend() {
    let source = Source()
    let (list, window) = fixture(source)
    var single = 0
    var first = 0
    var replacement = 0
    list.viewabilityConfigCallbackPairs = []
    list.viewabilityConfig = config()
    list.onViewableItemsChanged = { _ in single += 1 }
    layout(list)
    XCTAssertEqual(single, 0)
    list.viewabilityConfigCallbackPairs = [ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config()) { info in
      first += 1
      XCTAssertTrue(info.viewableItems.allSatisfy { $0.key.hasPrefix("public-") && $0.section == NSNotFound })
    }]
    XCTAssertEqual(first, 1)
    list.viewabilityConfigCallbackPairs = [ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config()) { _ in replacement += 1 }]
    layout(list)
    XCTAssertEqual(replacement, 0)
    list.contentOffset.y = 100
    layout(list)
    XCTAssertEqual(replacement, 1)
    XCTAssertEqual(single, 0)
    list.viewabilityConfigCallbackPairs = nil
    XCTAssertEqual(single, 0) // Equal effective config preserves the emitted keys.
    withExtendedLifetime((window, source)) {}
  }

  func testMinimumTimeAndInteractionUseWallClockAndProgrammaticScrollDoesNotUnlock() {
    let source = Source()
    let (list, window) = fixture(source)
    var times: [TimeInterval] = []
    list.viewabilityConfig = config(time: 100, wait: true)
    list.onViewableItemsChanged = { _ in times.append(Date.timeIntervalSinceReferenceDate) }
    list.contentOffset.y = 100
    layout(list)
    pump(0.14)
    XCTAssertTrue(times.isEmpty)
    let interaction = Date.timeIntervalSinceReferenceDate
    list.recordInteraction()
    pump(0.035)
    XCTAssertTrue(times.isEmpty)
    pump(0.14)
    XCTAssertEqual(times.count, 1)
    XCTAssertGreaterThanOrEqual((times.first ?? 0) - interaction, 0.095)
    withExtendedLifetime((window, source)) {}
  }

  func testReplacementAndDetachCancelPendingDeliveryAndReattachResetsInteraction() {
    let source = Source()
    let (list, window) = fixture(source)
    var calls = 0
    list.viewabilityConfig = config(time: 80)
    list.onViewableItemsChanged = { _ in calls += 1 }
    list.viewabilityConfig = config(time: 300, wait: true)
    pump(0.12)
    XCTAssertEqual(calls, 0)
    list.recordInteraction()
    list.removeFromSuperview()
    pump(0.35)
    XCTAssertEqual(calls, 0)
    window.addSubview(list)
    layout(list)
    pump(0.35)
    XCTAssertEqual(calls, 0)
    list.recordInteraction()
    pump(0.36)
    XCTAssertEqual(calls, 1)
    withExtendedLifetime((window, source)) {}
  }

  func testCallbackCanReplacePairsAndDetachWithoutStaleFollowups() {
    let source = Source()
    let (list, window) = fixture(source)
    var first = 0
    var stale = 0
    list.viewabilityConfigCallbackPairs = [
      ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config()) { _ in
        first += 1
        list.reloadData()
        list.viewabilityConfigCallbackPairs = []
        list.removeFromSuperview()
      },
      ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config(75)) { _ in stale += 1 }
    ]
    pump(0.05)
    XCTAssertEqual(first, 1)
    XCTAssertEqual(stale, 0)
    withExtendedLifetime((window, source)) {}
  }

  func testMasonryFiltersNonqualifyingRowsInsideBoundingRange() {
    let source = Source()
    source.heights = [100, 60, 50, 100, 100, 100]
    let (list, window) = fixture(source)
    list.numberOfColumns = 2
    list.reloadData()
    layout(list)
    list.contentOffset.y = 50
    layout(list)
    var keys: [String] = []
    list.viewabilityConfig = config()
    list.onViewableItemsChanged = { info in keys = info.viewableItems.map(\.key) }
    XCTAssertTrue(keys.contains("public-0"), "offset \(list.contentOffset.y), inset \(list.adjustedContentInset), bounds \(list.bounds), rows \((0..<3).map { list.rectForItem(at: $0) }), keys \(keys)")
    XCTAssertFalse(keys.contains("public-1"))
    XCTAssertTrue(keys.contains("public-2"))
    withExtendedLifetime((window, source)) {}
  }
  func testAdjustedInsetsUseTheObservedLogicalViewport() {
    let source = Source()
    let (list, window) = fixture(source, automaticInsets: true)
    list.contentInset.top = 17
    list.contentOffset.y = 50
    layout(list)
    let inset = list.adjustedContentInset
    let low = list.contentOffset.y + inset.top
    let high = low + list.bounds.height - inset.top - inset.bottom
    XCTAssertGreaterThan(inset.top, 0)
    XCTAssertGreaterThan(high, low)
    let expected = (0..<source.heights.count).filter {
      let rect = list.rectForItem(at: $0)
      return rect.maxY > low && rect.minY < high
    }.map { "public-\($0)" }
    var actual: [String] = []
    list.onViewableItemsChanged = { actual = $0.viewableItems.map(\.key) }
    XCTAssertFalse(expected.isEmpty)
    XCTAssertEqual(actual, expected)
    withExtendedLifetime((window, source)) {}
  }

  func testSectionsReportPublicItemsAndPreviousMetadataForRemovedTokens() {
    let source = SectionSource()
    let (list, window) = fixture(source)
    var info: ShadowListKitViewableItemsChangedInfo?
    list.onViewableItemsChanged = { info = $0 }
    XCTAssertEqual(info?.viewableItems.map(\.key), ["public-0", "public-1"])
    XCTAssertTrue(info?.viewableItems.allSatisfy { $0.section == 0 } == true)
    list.contentOffset.y = 120
    layout(list)
    XCTAssertEqual(info?.viewableItems.map(\.key), ["public-2", "public-3"])
    XCTAssertTrue(info?.viewableItems.allSatisfy { $0.section == 1 } == true)
    let removed = info?.changed.filter { !$0.isViewable } ?? []
    XCTAssertEqual(removed.map(\.index), [0, 1])
    XCTAssertTrue(removed.allSatisfy { $0.section == 0 && $0.key.hasPrefix("public-") })
    withExtendedLifetime((window, source)) {}
  }

  func testRemovalAfterReorderRetainsTheLastEmittedPublicIdentity() {
    let source = StableSectionSource()
    let (list, window) = fixture(source)
    var info: ShadowListKitViewableItemsChangedInfo?
    list.onViewableItemsChanged = { info = $0 }
    source.keys.swapAt(0, 1)
    list.reloadData()
    layout(list)
    source.keys.remove(at: 1)
    source.heights.remove(at: 1)
    source.firstCount = 1
    list.reloadData()
    layout(list)
    let removed = info?.changed.filter { !$0.isViewable } ?? []
    XCTAssertEqual(removed.map(\.key), ["public-0"])
    XCTAssertEqual(removed.map(\.index), [0])
    XCTAssertEqual(removed.map(\.section), [0])
    XCTAssertTrue(info?.viewableItems.allSatisfy { $0.key.hasPrefix("public-") } == true)
    withExtendedLifetime((window, source)) {}
  }

  func testUserScrollClassificationUnlocksWaitingWithoutAProgrammaticUnlock() {
    let source = Source()
    let (base, window) = fixture(source)
    let list = base as! List
    var calls = 0
    list.viewabilityConfig = config(wait: true)
    list.onViewableItemsChanged = { _ in calls += 1 }
    list.contentOffset.y = 50
    layout(list)
    XCTAssertEqual(calls, 0)
    list.draggingPhase = true
    list.contentOffset.y = 100
    layout(list)
    list.draggingPhase = false
    XCTAssertEqual(calls, 1)
    withExtendedLifetime((window, source)) {}
  }

  func testReloadOnlyDeliversTheImmutableFrameBeforeItsNextFrame() {
    let source = Source()
    let (list, window) = fixture(source)
    var events: [String] = []
    var reloaded = false
    list.viewabilityConfigCallbackPairs = [
      ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config()) { info in
        events.append("first:" + info.viewableItems.map(\.key).joined(separator: ","))
        if !reloaded {
          reloaded = true
          source.keys.removeFirst()
          source.heights.removeFirst()
          list.reloadData()
        }
      },
      ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config()) { info in
        events.append("second:" + info.viewableItems.map(\.key).joined(separator: ","))
      }
    ]
    layout(list)
    XCTAssertEqual(events, ["first:public-0", "second:public-0", "first:public-1", "second:public-1"])
    withExtendedLifetime((window, source)) {}
  }

  func testDetachAndReattachInsideACallbackDiscardsTheOldReports() throws {
    let source = Source()
    let (list, window) = fixture(source)
    let container = try XCTUnwrap(list.superview)
    let config = ShadowListKitViewabilityConfig(itemVisiblePercentThreshold: 50,
      viewAreaCoveragePercentThreshold: nil, minimumViewTime: 0, waitForInteraction: false)
    var events: [String] = []
    list.viewabilityConfigCallbackPairs = [
      ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config) { _ in
        events.append("first")
        if events.count == 1 {
          list.removeFromSuperview()
          container.addSubview(list)
        }
      },
      ShadowListKitViewabilityConfigCallbackPair(viewabilityConfig: config) { _ in events.append("second") }
    ]
    XCTAssertEqual(events, ["first", "first", "second"])
    withExtendedLifetime((window, source)) {}
  }

}
