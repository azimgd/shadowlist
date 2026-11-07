import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * A host drives the list the way a two axis grid does: it sets the offset and the phase, and
 * applies the offsets the list asks for.
 */
@MainActor
final class DrivenModeTests: XCTestCase {
  private final class WheelCatcher: NSView {
    var wheels = 0

    override func scrollWheel(with event: NSEvent) {
      wheels += 1
    }
  }

  private final class PhaseDelegate: NSObject, ShadowListKitListViewDelegate {
    var events: [String] = []

    func listViewWillBeginDragging(_ listView: ShadowListKitListView) { events.append("begin") }
    func listViewDidEndDragging(_ listView: ShadowListKitListView, willDecelerate decelerate: Bool) {
      events.append(decelerate ? "end-decelerate" : "end")
    }
    func listViewDidEndDecelerating(_ listView: ShadowListKitListView) { events.append("stop") }
  }

  private func makeDriven(count: Int = 600) -> ListHarness {
    let source = SizedTestSource(count: count)
    for (index, key) in source.keys.enumerated() {
      source.heights[key] = 24 + CGFloat(index % 4) * 8
    }
    return ListHarness(source: source) {
      $0.scrollDriven = true
      $0.estimatedItemSize = 30
      $0.allowsSelection = false
    }
  }

  /*
   * The host moves the offset by hand in steps while the phase says dragging.
   */
  private func drag(_ harness: ListHarness, to y: CGFloat, step: CGFloat = 120) {
    let list = harness.list
    list.scrollPhase = .dragging
    var current = list.contentOffset.y
    while abs(current - y) > 0.01 {
      current = y > current ? min(y, current + step) : max(y, current - step)
      list.contentOffset = CGPoint(x: 0, y: current)
      harness.layout()
      current = list.contentOffset.y
    }
  }

  func testHostOffsetAndPhaseMoveTheRows() throws {
    let harness = makeDriven()
    let list = harness.list
    drag(harness, to: 2400)
    XCTAssertTrue(list.isDragging)
    XCTAssertTrue(list.isTracking)
    let first = list.visibleRange.location
    let rect = list.rectForItem(at: first)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: first)), rect.minY - list.contentOffset.y, accuracy: 0.5)
    XCTAssertLessThanOrEqual(rect.minY, list.contentOffset.y)
    XCTAssertGreaterThan(rect.maxY, list.contentOffset.y)
  }

  func testCorrectionWhileDraggingIsReportedNotApplied() {
    let harness = makeDriven()
    let list = harness.list
    let source = harness.source
    drag(harness, to: 3000)
    harness.delegate.corrections.removeAll()
    harness.delegate.appliesCorrections = false
    let before = harness.visibleKeys()
    source.keys.insert(contentsOf: (0..<10).map { "above-\($0)" }, at: 0)
    list.insertItems(at: IndexSet(0..<10))
    harness.layout()
    XCTAssertFalse(harness.delegate.corrections.isEmpty)
    XCTAssertEqual(list.contentOffset.y, 3000, accuracy: 0.01)
    let wanted = harness.delegate.corrections.last!.y
    XCTAssertGreaterThan(wanted, 3000)

    // Once the host applies it the rows on screen are back where they were.
    harness.delegate.appliesCorrections = true
    list.contentOffset = CGPoint(x: 0, y: wanted)
    harness.layout()
    let (shift, kept) = harness.maxShift(from: before)
    XCTAssertLessThan(shift, 0.5)
    XCTAssertEqual(kept, before.count)
  }

  func testAppliedCorrectionsKeepRowsStillThroughBatchUpdates() {
    let harness = makeDriven()
    let list = harness.list
    let source = harness.source
    drag(harness, to: 2000)
    let before = harness.visibleKeys()
    list.performBatchUpdates({
      source.keys.removeSubrange(0..<6)
      source.keys.insert(contentsOf: ["x", "y", "z"], at: 0)
      list.deleteItems(at: IndexSet(0..<6))
      list.insertItems(at: IndexSet(0..<3))
    }, completion: nil)
    harness.layout()
    XCTAssertFalse(harness.delegate.corrections.isEmpty)
    let (shift, kept) = harness.maxShift(from: before)
    XCTAssertLessThan(shift, 0.5)
    XCTAssertEqual(kept, before.count)
  }

  func testSettlingFollowsDraggingAndEndsIdle() {
    let harness = makeDriven()
    let list = harness.list
    let phases = PhaseDelegate()
    drag(harness, to: 600)
    list.delegate = phases
    list.scrollPhase = .settling
    XCTAssertTrue(list.isDecelerating)
    XCTAssertFalse(list.isDragging)
    for step in 1...5 {
      list.contentOffset = CGPoint(x: 0, y: 600 + CGFloat(step) * 40)
      harness.layout()
    }
    list.scrollPhase = .idle
    harness.layout()
    XCTAssertEqual(phases.events, ["end-decelerate", "stop"])
    XCTAssertEqual(list.contentOffset.y, 800, accuracy: 0.5)
    XCTAssertNotNil(list.cellForItem(at: list.visibleRange.location))
  }

  func testScrollCommandAsksTheHostForTheOffset() throws {
    let harness = makeDriven(count: 5000)
    let list = harness.list
    list.scrollToItem(at: 3000, viewPosition: 0, animated: false)
    harness.layout()
    XCTAssertFalse(harness.delegate.corrections.isEmpty)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 3000)), 0, accuracy: 0.5)
  }

  func testWheelGoesToTheNextResponder() throws {
    let harness = makeDriven()
    let catcher = WheelCatcher(frame: harness.window.contentView!.bounds)
    let list = harness.list
    list.removeFromSuperview()
    catcher.addSubview(list)
    harness.window.contentView!.addSubview(catcher)
    let cgEvent = try XCTUnwrap(CGEvent(scrollWheelEvent2Source: nil, units: .pixel, wheelCount: 1, wheel1: -40, wheel2: 0, wheel3: 0))
    let event = try XCTUnwrap(NSEvent(cgEvent: cgEvent))
    list.scrollWheel(with: event)
    XCTAssertEqual(catcher.wheels, 1)
    XCTAssertEqual(list.contentOffset.y, 0)
  }
}
