import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * The scroll engine on its own, the way a host that owns its scroll position uses it.
 */
@MainActor
final class ScrollEngineTests: XCTestCase {
  private final class EngineDelegate: NSObject, ShadowListKitScrollEngineDelegate {
    var offsets: [CGPoint] = []
    var phases: [ShadowListKitScrollPhase] = []
    var animationsEnded = 0
    var resting: ((CGPoint) -> CGPoint)?

    func scrollEngine(_ engine: ShadowListKitScrollEngine, didScrollTo offset: CGPoint) {
      offsets.append(offset)
    }

    func scrollEngine(_ engine: ShadowListKitScrollEngine, didChange phase: ShadowListKitScrollPhase) {
      phases.append(phase)
    }

    func scrollEngineDidEndAnimation(_ engine: ShadowListKitScrollEngine) {
      animationsEnded += 1
    }

    func scrollEngine(_ engine: ShadowListKitScrollEngine, restingOffsetFor offset: CGPoint) -> CGPoint {
      resting?(offset) ?? offset
    }
  }

  private var view: NSView!
  private var delegate: EngineDelegate!

  /*
   * A 300 by 500 viewport over 1300 by 2000 of content: offsets up to 1000 and 1500.
   */
  private func makeEngine(axes: ShadowListKitScrollAxes = .vertical) -> ShadowListKitScrollEngine {
    view = NSView(frame: NSRect(x: 0, y: 0, width: 300, height: 500))
    delegate = EngineDelegate()
    let engine = ShadowListKitScrollEngine(view: view)
    engine.axes = axes
    engine.delegate = delegate
    engine.viewportSize = CGSize(width: 300, height: 500)
    engine.contentSize = CGSize(width: 1300, height: 2000)
    return engine
  }

  private func send(_ engine: ShadowListKitScrollEngine, _ delta: CGPoint, _ phase: NSEvent.Phase, _ momentum: NSEvent.Phase = []) -> Bool {
    engine.handleScrollWheel(ShadowListKitScrollWheelEvent(delta, phase, momentum))
  }

  private func lineEvent(_ lines: Int32) throws -> NSEvent {
    let cgEvent = try XCTUnwrap(CGEvent(scrollWheelEvent2Source: nil, units: .line, wheelCount: 1, wheel1: lines, wheel2: 0, wheel3: 0))
    return try XCTUnwrap(NSEvent(cgEvent: cgEvent))
  }

  func testPhasedWheelDragsAndItsMomentumSettles() {
    let engine = makeEngine()
    XCTAssertTrue(send(engine, .zero, .began))
    XCTAssertEqual(engine.phase, .idle)
    for _ in 0..<5 {
      XCTAssertTrue(send(engine, CGPoint(x: 0, y: -20), .changed))
    }
    XCTAssertEqual(engine.phase, .dragging)
    XCTAssertEqual(engine.offset.y, 100, accuracy: 0.01)
    _ = send(engine, .zero, .ended)
    _ = send(engine, CGPoint(x: 0, y: -30), [], .began)
    XCTAssertEqual(engine.phase, .settling)
    _ = send(engine, CGPoint(x: 0, y: -10), [], .changed)
    _ = send(engine, .zero, [], .ended)
    XCTAssertEqual(engine.offset.y, 140, accuracy: 0.01)
    XCTAssertEqual(delegate.phases, [.dragging, .settling, .idle])
    XCTAssertEqual(delegate.offsets.last?.y ?? 0, 140, accuracy: 0.01)
  }

  func testMomentumPastTheEdgeSpringsBack() {
    let engine = makeEngine()
    engine.offset = CGPoint(x: 0, y: 1480)
    _ = send(engine, .zero, .began)
    _ = send(engine, CGPoint(x: 0, y: -10), .changed)
    _ = send(engine, .zero, .ended)
    _ = send(engine, CGPoint(x: 0, y: -40), [], .began)
    var deepest: CGFloat = 0
    for _ in 0..<20 {
      _ = send(engine, CGPoint(x: 0, y: -40), [], .changed)
      deepest = max(deepest, engine.offset.y)
      engine.shadowListKit_runFrames(for: 1.0 / 120)
    }
    _ = send(engine, .zero, [], .ended)
    XCTAssertGreaterThan(deepest, 1500)
    XCTAssertLessThanOrEqual(deepest, 1500 + 500 * 0.12)
    engine.shadowListKit_runFrames(for: 0.6)
    XCTAssertEqual(engine.offset.y, 1500)
    XCTAssertEqual(engine.phase, .idle)
  }

  func testDragPastTheEdgeRubberBandsAndSpringsBack() {
    let engine = makeEngine()
    _ = send(engine, .zero, .began)
    for _ in 0..<10 {
      _ = send(engine, CGPoint(x: 0, y: 40), .changed)
    }
    let pulled = engine.offset.y
    XCTAssertLessThan(pulled, -40)
    XCTAssertGreaterThan(pulled, -250)
    _ = send(engine, .zero, .ended)
    XCTAssertEqual(engine.phase, .settling)
    engine.shadowListKit_runFrames(for: 0.6)
    XCTAssertEqual(engine.offset.y, 0)
    XCTAssertEqual(engine.phase, .idle)
  }

  func testTwoAxesLockToTheAxisAGestureStartsWithin30Degrees() {
    let engine = makeEngine(axes: [.horizontal, .vertical])
    _ = send(engine, .zero, .began)
    // 20 degrees off vertical moves only the vertical axis, for the whole gesture.
    _ = send(engine, CGPoint(x: -4, y: -11), .changed)
    _ = send(engine, CGPoint(x: -40, y: -10), .changed)
    XCTAssertEqual(engine.offset.x, 0)
    XCTAssertEqual(engine.offset.y, 21, accuracy: 0.01)
    _ = send(engine, .zero, .ended)
    engine.shadowListKit_runFrames(for: 0.2)
    _ = send(engine, .zero, .began)
    _ = send(engine, CGPoint(x: -10, y: -10), .changed)
    XCTAssertEqual(engine.offset.x, 10, accuracy: 0.01)
    XCTAssertEqual(engine.offset.y, 31, accuracy: 0.01)
  }

  func testMouseWheelScrollsByLinesAndStopsAtTheEdges() throws {
    let engine = makeEngine()
    engine.lineScroll = 20
    XCTAssertTrue(engine.handleScrollWheel(try lineEvent(-3)))
    XCTAssertEqual(engine.offset.y, 60, accuracy: 0.01)
    XCTAssertEqual(engine.phase, .dragging)
    XCTAssertTrue(engine.handleScrollWheel(try lineEvent(10)))
    XCTAssertEqual(engine.offset.y, 0)
    engine.shadowListKit_runFrames(for: 0.3)
    XCTAssertEqual(engine.phase, .idle)
    // At the edge nothing moves, no rubber band, and the event is passed on.
    XCTAssertFalse(engine.handleScrollWheel(try lineEvent(2)))
    XCTAssertEqual(engine.offset.y, 0)
    for _ in 0..<200 {
      _ = engine.handleScrollWheel(try lineEvent(-5))
    }
    XCTAssertEqual(engine.offset.y, 1500)
  }

  func testGestureAcrossOneAxisIsPassedOnWithItsMomentum() {
    let engine = makeEngine()
    XCTAssertTrue(send(engine, .zero, .began))
    XCTAssertFalse(send(engine, CGPoint(x: -30, y: -5), .changed))
    XCTAssertFalse(send(engine, CGPoint(x: 0, y: -30), .changed))
    XCTAssertFalse(send(engine, .zero, .ended))
    XCTAssertFalse(send(engine, CGPoint(x: -20, y: 0), [], .began))
    XCTAssertFalse(send(engine, .zero, [], .ended))
    XCTAssertEqual(engine.offset.y, 0)
    XCTAssertTrue(delegate.phases.isEmpty)
  }

  func testAnimateToOffsetLandsExactly() {
    let engine = makeEngine(axes: [.horizontal, .vertical])
    engine.animate(to: CGPoint(x: 333.3, y: 1234.5))
    XCTAssertTrue(engine.isAnimating)
    engine.shadowListKit_runFrames(for: 0.15)
    XCTAssertGreaterThan(engine.offset.y, 600)
    XCTAssertLessThan(engine.offset.y, 1234.5)
    XCTAssertEqual(engine.phase, .idle)
    engine.shadowListKit_runFrames(for: 0.3)
    XCTAssertEqual(engine.offset, CGPoint(x: 333.3, y: 1234.5))
    XCTAssertFalse(engine.isAnimating)
    XCTAssertEqual(delegate.animationsEnded, 1)
    // A gesture cuts an animation short without the call.
    engine.animate(to: CGPoint(x: 0, y: 0))
    engine.shadowListKit_runFrames(for: 0.1)
    _ = send(engine, .zero, .mayBegin)
    engine.shadowListKit_runFrames(for: 0.5)
    XCTAssertGreaterThan(engine.offset.y, 0)
    XCTAssertEqual(delegate.animationsEnded, 1)
  }

  func testRestingOffsetSnapsTheEndOfAGesture() throws {
    let engine = makeEngine()
    delegate.resting = { CGPoint(x: $0.x, y: ($0.y / 100).rounded() * 100) }
    _ = engine.handleScrollWheel(try lineEvent(-9))
    XCTAssertEqual(engine.offset.y, 144, accuracy: 0.01)
    engine.shadowListKit_runFrames(for: 0.2)
    XCTAssertEqual(engine.phase, .settling)
    engine.shadowListKit_runFrames(for: 0.4)
    XCTAssertEqual(engine.offset.y, 100)
    XCTAssertEqual(engine.phase, .idle)
    XCTAssertEqual(delegate.phases, [.dragging, .settling, .idle])
    XCTAssertEqual(delegate.animationsEnded, 0)
  }

  func testStopScrollingDropsTheRestOfTheMomentum() {
    let engine = makeEngine()
    _ = send(engine, .zero, .began)
    _ = send(engine, CGPoint(x: 0, y: -20), .changed)
    _ = send(engine, .zero, .ended)
    _ = send(engine, CGPoint(x: 0, y: -20), [], .began)
    engine.stopScrolling()
    XCTAssertEqual(engine.phase, .idle)
    XCTAssertTrue(send(engine, CGPoint(x: 0, y: -20), [], .changed))
    XCTAssertEqual(engine.offset.y, 40, accuracy: 0.01)
  }
}
