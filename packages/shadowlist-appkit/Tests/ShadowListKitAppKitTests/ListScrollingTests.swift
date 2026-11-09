import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * A list that is not driven scrolls itself: wheel gestures, animated commands, snapping, a
 * horizontal axis, scrollers and the wheel it passes on.
 */
@MainActor
final class ListScrollingTests: XCTestCase {
  private final class WheelCatcher: NSView {
    var phases: [NSEvent.Phase] = []
    var momentumPhases: [NSEvent.Phase] = []

    override func scrollWheel(with event: NSEvent) {
      phases.append(event.phase)
      momentumPhases.append(event.momentumPhase)
    }
  }

  private final class ScrollDelegate: NSObject, ShadowListKitListViewDelegate {
    var events: [String] = []

    func listViewWillBeginDragging(_ listView: ShadowListKitListView) { events.append("begin") }
    func listViewDidEndDragging(_ listView: ShadowListKitListView, willDecelerate decelerate: Bool) {
      events.append(decelerate ? "end-decelerate" : "end")
    }
    func listViewDidEndDecelerating(_ listView: ShadowListKitListView) { events.append("stop") }
    func listViewDidEndScrollingAnimation(_ listView: ShadowListKitListView) { events.append("animation") }
  }

  private let scrollDelegate = ScrollDelegate()

  private func makeHarness(count: Int = 400, configure: (ShadowListKitListView) -> Void = { _ in }) -> ListHarness {
    let harness = ListHarness(source: SizedTestSource(count: count), configure: configure)
    harness.list.delegate = scrollDelegate
    return harness
  }

  private func wheel(_ harness: ListHarness, _ delta: CGPoint, _ phase: NSEvent.Phase, _ momentum: NSEvent.Phase = []) {
    harness.list.shadowListKit_scrollWheelDelta(delta, phase: phase, momentumPhase: momentum)
  }

  /*
   * Puts the list in a view that records the wheel events passed to it.
   */
  private func catchWheel(_ harness: ListHarness) -> WheelCatcher {
    let catcher = WheelCatcher(frame: harness.window.contentView!.bounds)
    let list = harness.list
    list.removeFromSuperview()
    catcher.addSubview(list)
    harness.window.contentView!.addSubview(catcher)
    harness.layout()
    return catcher
  }

  func testTrackpadGestureDragsAndSettlesWithItsMomentum() {
    let harness = makeHarness()
    let list = harness.list
    wheel(harness, .zero, .began)
    for _ in 0..<10 {
      wheel(harness, CGPoint(x: 0, y: -30), .changed)
    }
    XCTAssertTrue(list.isDragging)
    wheel(harness, .zero, .ended)
    wheel(harness, CGPoint(x: 0, y: -40), [], .began)
    XCTAssertTrue(list.isDecelerating)
    for _ in 0..<10 {
      wheel(harness, CGPoint(x: 0, y: -20), [], .changed)
    }
    wheel(harness, .zero, [], .ended)
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, 540, accuracy: 0.01)
    XCTAssertEqual(list.scrollPhase, .idle)
    XCTAssertEqual(scrollDelegate.events, ["begin", "end-decelerate", "stop"])
    let first = list.visibleRange.location
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: first)), list.rectForItem(at: first).minY - 540, accuracy: 0.5)
  }

  func testBounceAtTheStartReturnsToTheEdge() {
    let harness = makeHarness()
    let list = harness.list
    wheel(harness, .zero, .began)
    for _ in 0..<8 {
      wheel(harness, CGPoint(x: 0, y: 40), .changed)
    }
    XCTAssertLessThan(list.contentOffset.y, -20)
    // The first row follows the pull.
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 0)), -list.contentOffset.y, accuracy: 0.5)
    wheel(harness, .zero, .ended)
    harness.runScrollFrames()
    XCTAssertEqual(list.contentOffset.y, 0)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 0)), 0, accuracy: 0.5)
    XCTAssertEqual(list.scrollPhase, .idle)
  }

  func testAnimatedScrollToItemLandsWithinHalfAPoint() throws {
    let source = TestSource(count: 3000)
    for (index, key) in source.keys.enumerated() {
      source.heights[key] = 28 + CGFloat((index * 7) % 11) * 9
    }
    let harness = ListHarness(source: source) { $0.estimatedItemSize = 60 }
    let list = harness.list
    list.delegate = scrollDelegate
    list.scrollToItem(at: 1800, viewPosition: 0, animated: true)
    harness.layout()
    // Still on its way.
    XCTAssertLessThan(list.contentOffset.y, 1000)
    list.shadowListKit_runScrollFrames(for: 0.1)
    XCTAssertGreaterThan(list.contentOffset.y, 1000)
    XCTAssertFalse(scrollDelegate.events.contains("animation"))
    harness.runScrollFrames()
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 1800)), 0, accuracy: 0.5)
    XCTAssertEqual(scrollDelegate.events, ["animation"])

    list.scrollToItem(at: 40, viewPosition: 0.5, animated: true)
    harness.runScrollFrames()
    let middle = try XCTUnwrap(harness.screenY(ofItem: 40)) + list.rectForItem(at: 40).height / 2
    XCTAssertEqual(middle, 240, accuracy: 0.5)

    list.scrollToEnd(animated: true)
    harness.runScrollFrames()
    XCTAssertEqual(list.contentOffset.y, list.contentSize.height - 480, accuracy: 0.5)
    XCTAssertEqual(scrollDelegate.events.filter { $0 == "animation" }.count, 3)
  }

  func testAnimatedContentOffsetEasesThere() {
    let harness = makeHarness()
    let list = harness.list
    list.setContentOffset(CGPoint(x: 0, y: 900), animated: true)
    list.shadowListKit_runScrollFrames(for: 0.1)
    XCTAssertGreaterThan(list.contentOffset.y, 0)
    XCTAssertLessThan(list.contentOffset.y, 900)
    harness.runScrollFrames()
    XCTAssertEqual(list.contentOffset.y, 900)
    XCTAssertEqual(scrollDelegate.events, ["animation"])
  }

  func testCommandStopsMomentum() {
    let harness = makeHarness()
    let list = harness.list
    wheel(harness, .zero, .began)
    wheel(harness, CGPoint(x: 0, y: -40), .changed)
    wheel(harness, .zero, .ended)
    wheel(harness, CGPoint(x: 0, y: -40), [], .began)
    list.scrollToItem(at: 100, viewPosition: 0, animated: false)
    harness.layout()
    let landed = list.contentOffset.y
    wheel(harness, CGPoint(x: 0, y: -40), [], .changed)
    wheel(harness, .zero, [], .ended)
    XCTAssertEqual(list.contentOffset.y, landed)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 100)), 0, accuracy: 0.5)
  }

  func testSnapToItemRestsOnAnItemEdge() {
    let harness = makeHarness { $0.snapToItem = true }
    let list = harness.list
    // An unphased wheel of 30 points rests on the nearer item edge, 44.
    wheel(harness, CGPoint(x: 0, y: -30), [])
    XCTAssertEqual(list.contentOffset.y, 30, accuracy: 0.01)
    harness.runScrollFrames()
    XCTAssertEqual(list.contentOffset.y, 44, accuracy: 0.01)
    XCTAssertEqual(list.scrollPhase, .idle)

    // Momentum that ends between edges settles on the nearest.
    wheel(harness, .zero, .began)
    wheel(harness, CGPoint(x: 0, y: -50), .changed)
    wheel(harness, .zero, .ended)
    wheel(harness, CGPoint(x: 0, y: -70), [], .began)
    wheel(harness, .zero, [], .ended)
    XCTAssertEqual(list.contentOffset.y, 164, accuracy: 0.01)
    XCTAssertTrue(list.isDecelerating)
    harness.runScrollFrames()
    XCTAssertEqual(list.contentOffset.y, 176, accuracy: 0.01)
    XCTAssertEqual(list.scrollPhase, .idle)
  }

  func testHorizontalListScrollsSidewaysAndPassesOnVerticalGestures() throws {
    let harness = makeHarness { $0.isHorizontal = true }
    let list = harness.list
    let catcher = catchWheel(harness)
    wheel(harness, .zero, .began)
    wheel(harness, CGPoint(x: -60, y: -5), .changed)
    wheel(harness, .zero, .ended)
    harness.runScrollFrames()
    XCTAssertEqual(list.contentOffset.x, 60, accuracy: 0.01)
    XCTAssertTrue(catcher.phases.isEmpty)
    let first = list.visibleRange.location
    XCTAssertEqual(try XCTUnwrap(harness.cellX(ofItem: first)), list.rectForItem(at: first).minX - 60, accuracy: 0.5)

    wheel(harness, .zero, .began)
    wheel(harness, CGPoint(x: -2, y: -40), .changed)
    wheel(harness, CGPoint(x: 0, y: -40), .changed)
    wheel(harness, .zero, .ended)
    wheel(harness, CGPoint(x: 0, y: -40), [], .began)
    wheel(harness, .zero, [], .ended)
    XCTAssertEqual(list.contentOffset.x, 60, accuracy: 0.01)
    XCTAssertEqual(catcher.phases, [.changed, .changed, .ended, [], []])
    XCTAssertEqual(catcher.momentumPhases, [[], [], [], .began, .ended])
  }

  func testGestureAcrossTheAxisGoesToTheNextResponderWithItsMomentum() {
    let harness = makeHarness()
    let list = harness.list
    let catcher = catchWheel(harness)
    wheel(harness, .zero, .began)
    wheel(harness, CGPoint(x: -40, y: -3), .changed)
    wheel(harness, CGPoint(x: -40, y: -30), .changed)
    wheel(harness, .zero, .ended)
    wheel(harness, CGPoint(x: -30, y: 0), [], .began)
    wheel(harness, .zero, [], .ended)
    XCTAssertEqual(catcher.phases.count, 5)
    XCTAssertEqual(list.contentOffset.y, 0)
    XCTAssertEqual(list.scrollPhase, .idle)
    XCTAssertTrue(scrollDelegate.events.isEmpty)
  }

  func testMouseWheelAtAnEdgeGoesToTheNextResponder() throws {
    let harness = makeHarness()
    let catcher = catchWheel(harness)
    let cgEvent = try XCTUnwrap(CGEvent(scrollWheelEvent2Source: nil, units: .line, wheelCount: 1, wheel1: 3, wheel2: 0, wheel3: 0))
    harness.list.scrollWheel(with: try XCTUnwrap(NSEvent(cgEvent: cgEvent)))
    XCTAssertEqual(catcher.phases.count, 1)
    XCTAssertEqual(harness.list.contentOffset.y, 0)
  }

  func testScrollersFollowTheListAndHideWhenDriven() {
    let harness = makeHarness()
    let list = harness.list
    let scrollers = list.shadowListKit_scrollerView
    scrollers.shadowListKit_setScrollerStyle(.overlay)
    XCTAssertEqual(scrollers.contentSize.height, list.contentSize.height)
    list.contentOffset = CGPoint(x: 0, y: 1000)
    harness.layout()
    let thumb = scrollers.shadowListKit_verticalThumbFrame
    XCTAssertFalse(thumb.isNull)
    let track: CGFloat = 476
    // Long content keeps the thumb at its least length.
    XCTAssertEqual(thumb.height, max(36, track * 480 / list.contentSize.height), accuracy: 0.5)
    XCTAssertEqual(thumb.minY - 2, 1000 / (list.contentSize.height - 480) * (track - thumb.height), accuracy: 0.5)
    list.isScrollDriven = true
    XCTAssertTrue(scrollers.isHidden)
  }
}

extension ListHarness {
  /*
   * The left edge of an item's cell in the list's own coordinates, or nil when it is not mounted.
   */
  func cellX(ofItem index: Int) -> CGFloat? {
    guard let cell = list.cellForItem(at: index), let superview = cell.superview else { return nil }
    return list.convert(cell.frame, from: superview).minX
  }
}
