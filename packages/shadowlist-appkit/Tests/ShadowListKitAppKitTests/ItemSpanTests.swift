import AppKit
import ShadowListKitAppKit
import XCTest

@MainActor
final class ItemSpanTests: XCTestCase {
  func testOffscreenSpansAndColumnChangesUseSharedPlacement() {
    let source = SizedTestSource(count: 200)
    var calls = 0
    // Divisible track widths isolate placement from the public pixel-rounded rect adaptation.
    let harness = ListHarness(source: source, size: NSSize(width: 360, height: 480)) { list in
      list.numberOfColumns = 3
      list.getItemSpan = { index in
        XCTAssertTrue(Thread.isMainThread)
        calls += 1
        return index % 3 + 1
      }
    }
    let width = harness.list.bounds.width
    XCTAssertEqual(calls, 200)
    XCTAssertEqual(harness.list.rectForItem(at: 0).width, width / 3, accuracy: 0.01)
    XCTAssertEqual(harness.list.rectForItem(at: 1).width, width * 2 / 3, accuracy: 0.01)
    XCTAssertEqual(harness.list.rectForItem(at: 197).width, width, accuracy: 0.01)
    harness.layout()
    XCTAssertEqual(calls, 200)
    harness.list.numberOfColumns = 2
    harness.layout()
    XCTAssertEqual(harness.list.rectForItem(at: 1).width, width, accuracy: 0.01)
    XCTAssertEqual(calls, 400)
    harness.list.getItemSpan = nil
    harness.layout()
    XCTAssertEqual(harness.list.rectForItem(at: 1).width, width / 2, accuracy: 0.01)
  }

  func testFractionalTrackEdgesShareTheSamePublicPixelBoundary() {
    let source = SizedTestSource(count: 20)
    let harness = ListHarness(source: source) { list in
      list.numberOfColumns = 3
      list.getItemSpan = { $0 % 3 + 1 }
    }
    for scale in [CGFloat(1), 2, 3] {
      harness.window.scale = scale
      harness.list.viewDidChangeBackingProperties()
      harness.layout()
      let first = harness.list.rectForItem(at: 0)
      let second = harness.list.rectForItem(at: 1)
      let full = harness.list.rectForItem(at: 2)
      let boundary = (harness.list.bounds.width / 3 * scale).rounded() / scale
      XCTAssertEqual(first.minX, 0)
      XCTAssertEqual(first.maxX, boundary)
      XCTAssertEqual(second.minX, boundary)
      XCTAssertEqual(second.maxX, harness.list.bounds.width)
      XCTAssertEqual(full.minX, 0)
      XCTAssertEqual(full.maxX, harness.list.bounds.width)
    }
  }

  func testMixedSpanArrowsFollowVisualNeighborsAndKeepSelectionPolicy() {
    let harness = SelectionHarness(count: 12) { list in
      list.numberOfColumns = 3
      list.getItemSpan = { $0 == 0 || $0 == 4 ? 3 : 1 }
    }
    harness.click(0)
    harness.press(.down)
    XCTAssertEqual(harness.selection, [2])
    harness.press(.right)
    XCTAssertEqual(harness.selection, [3])
    harness.press(.up)
    XCTAssertEqual(harness.selection, [0])
    harness.recorder.refused = [2]
    harness.press(.down)
    XCTAssertEqual(harness.selection, [1])
    harness.press(.down)
    XCTAssertEqual(harness.selection, [4])
  }

  func testSelectionCallbackRemovingTheCurrentItemCancelsOldGeometryNavigation() {
    let harness = SelectionHarness(count: 12) { list in
      list.numberOfColumns = 3
      list.getItemSpan = { $0 == 0 ? 3 : 1 }
    }
    harness.click(0)
    var changed = false
    harness.recorder.onShouldSelect = { [weak list = harness.list, weak source = harness.source] _ in
      guard !changed else { return }
      changed = true
      source?.keys.remove(at: 0)
      list?.reloadData()
    }
    harness.press(.down)
    XCTAssertTrue(changed)
    XCTAssertTrue(harness.selection.isEmpty)
    harness.recorder.onShouldSelect = nil
  }
}
