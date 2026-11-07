import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Scroll commands and the saved position.
 */
@MainActor
final class CommandsTests: XCTestCase {
  private func makeHarness(count: Int = 5000, estimate: CGFloat = 120) -> ListHarness {
    let source = TestSource(count: count)
    for (index, key) in source.keys.enumerated() {
      source.heights[key] = 28 + CGFloat((index * 7) % 11) * 9
    }
    return ListHarness(source: source) { $0.estimatedItemSize = estimate }
  }

  func testScrollToItemLandsAtItsViewPositionAcrossEstimates() throws {
    for estimate: CGFloat in [20, 60, 120, 400] {
      let harness = makeHarness(estimate: estimate)
      let list = harness.list
      list.scrollToItem(at: 3000, viewPosition: 0, animated: false)
      harness.layout()
      XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 3000)), 0, accuracy: 0.5, "estimate \(estimate)")

      list.scrollToItem(at: 1200, viewPosition: 1, animated: true)
      harness.runScrollFrames()
      let rect = list.rectForItem(at: 1200)
      XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 1200)) + rect.height, 480, accuracy: 0.5, "estimate \(estimate)")

      list.scrollToItem(at: 4200, viewPosition: 0.5, animated: false)
      harness.layout()
      let middle = try XCTUnwrap(harness.screenY(ofItem: 4200)) + list.rectForItem(at: 4200).height / 2
      XCTAssertEqual(middle, 240, accuracy: 0.5, "estimate \(estimate)")
    }
  }

  func testScrollToEndAndStart() throws {
    let harness = makeHarness(count: 800)
    let list = harness.list
    list.scrollToEnd(animated: false)
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, list.contentSize.height - 480, accuracy: 0.5)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 799)) + list.rectForItem(at: 799).height, 480, accuracy: 0.5)
    list.scrollToStart(animated: false)
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, 0, accuracy: 0.01)
  }

  func testAnchorStateRoundTrip() throws {
    let harness = makeHarness(count: 2000)
    let list = harness.list
    list.scrollToItem(at: 700, viewPosition: 0, animated: false)
    list.contentOffset = CGPoint(x: 0, y: list.contentOffset.y + 13)
    harness.layout()
    let state = try XCTUnwrap(list.anchorState)
    XCTAssertEqual(state.key, "item-700")
    XCTAssertEqual(state.offset, 13, accuracy: 0.5)
    let y = try XCTUnwrap(harness.screenY(ofItem: 700))

    list.scrollToItem(at: 20, viewPosition: 0, animated: false)
    harness.layout()
    list.restoreAnchorState(state)
    harness.layout()
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 700)), y, accuracy: 0.5)
  }

  func testAnchorStateWaitsForItsKey() throws {
    let harness = makeHarness(count: 100)
    let list = harness.list
    list.restoreAnchorState(ShadowListKitAnchorState(key: "later-5", offset: 4))
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, 0)
    harness.source.keys.append(contentsOf: (0..<50).map { "later-\($0)" })
    list.reloadData()
    harness.layout()
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 105)), -4, accuracy: 0.5)
  }

  func testRestorableStateCarriesThePosition() throws {
    let saved = makeHarness(count: 1000)
    saved.list.identifier = NSUserInterfaceItemIdentifier("list")
    saved.list.scrollToItem(at: 400, viewPosition: 0, animated: false)
    saved.layout()
    let archiver = NSKeyedArchiver(requiringSecureCoding: true)
    saved.list.encodeRestorableState(with: archiver)
    archiver.finishEncoding()

    let restored = makeHarness(count: 1000)
    restored.list.identifier = NSUserInterfaceItemIdentifier("list")
    let unarchiver = try NSKeyedUnarchiver(forReadingFrom: archiver.encodedData)
    restored.list.restoreState(with: unarchiver)
    restored.layout()
    XCTAssertEqual(try XCTUnwrap(restored.screenY(ofItem: 400)), 0, accuracy: 0.5)
  }
}
