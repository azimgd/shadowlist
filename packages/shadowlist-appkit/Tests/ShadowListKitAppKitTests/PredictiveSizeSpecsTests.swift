import AppKit
import ShadowListKitAppKit
import XCTest

@MainActor
final class PredictiveSizeSpecsTests: XCTestCase {
  func testPredictedTextAndFixedHeightUseSharedSourceWithoutReplacingExactSizes() {
    let source = SizedTestSource(count: 200)
    let harness = ListHarness(source: source) { list in
      list.getItemSizeSpec = { _ in
        XCTAssertTrue(Thread.isMainThread)
        let spec = ShadowListKitItemSizeSpec(text: "prediction")
        spec.fixedHeight = 180
        return spec
      }
    }
    XCTAssertEqual(harness.list.rectForItem(at: 40).height, 180)
    XCTAssertEqual(harness.list.rectForItem(at: 0).height, 44)
    harness.list.getItemSizeSpec = nil
    harness.layout()
    XCTAssertEqual(harness.list.rectForItem(at: 40).height, 44)
    XCTAssertEqual(harness.list.rectForItem(at: 0).height, 44)
  }
}
