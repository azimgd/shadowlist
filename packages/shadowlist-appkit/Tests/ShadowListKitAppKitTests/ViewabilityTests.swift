import AppKit
import ShadowListKitAppKit
import XCTest

@MainActor
final class ViewabilityTests: XCTestCase {
  func testOffscreenHostWaitsForInteractionAndCancelsTimerWhenDetached() {
    let harness = ListHarness(source: SizedTestSource(count: 100))
    let list = harness.list
    var calls = 0
    list.viewabilityConfig = ShadowListKitViewabilityConfig(itemVisiblePercentThreshold: 50,
      viewAreaCoveragePercentThreshold: nil, minimumViewTime: 80, waitForInteraction: true)
    list.onViewableItemsChanged = { info in
      calls += 1
      XCTAssertTrue(info.viewableItems.allSatisfy { $0.section == NSNotFound && $0.isViewable })
    }
    RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.12))
    XCTAssertEqual(calls, 0)
    list.recordInteraction()
    RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.03))
    XCTAssertEqual(calls, 0)
    RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.12))
    XCTAssertEqual(calls, 1)
    list.viewabilityConfig = ShadowListKitViewabilityConfig(itemVisiblePercentThreshold: 75,
      viewAreaCoveragePercentThreshold: nil, minimumViewTime: 100, waitForInteraction: false)
    list.removeFromSuperview()
    RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.15))
    XCTAssertEqual(calls, 1)
  }
}
