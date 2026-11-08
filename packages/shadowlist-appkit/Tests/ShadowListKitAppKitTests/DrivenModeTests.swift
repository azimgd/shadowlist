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
      $0.isScrollDriven = true
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
  /*
   * A move that keeps the same rows mounted runs no layout pass: the content moves by its layer.
   * Once a row comes in a pass mounts it.
   */
  func testAMoveInsideTheMountedRowsRunsNoPass() throws {
    let harness = makeDriven()
    let list = harness.list
    drag(harness, to: 1200)
    list.scrollPhase = .idle
    harness.layout()
    // The pass after a correction settled publishes the band.
    list.contentOffset = CGPoint(x: 0, y: 1201)
    list.layoutSubtreeIfNeeded()
    let passes = list.shadowListKit_layoutPassCount
    let mounts = list.shadowListKit_mountPassCount
    let cores = list.shadowListKit_corePassCount
    for step in 2...4 {
      list.contentOffset = CGPoint(x: 0, y: 1200 + CGFloat(step))
      XCTAssertFalse(list.needsLayout)
      list.layoutSubtreeIfNeeded()
    }
    XCTAssertEqual(list.shadowListKit_layoutPassCount, passes)
    XCTAssertEqual(list.shadowListKit_mountPassCount, mounts)
    XCTAssertEqual(list.shadowListKit_corePassCount, cores)
    let first = list.visibleRange.location
    let screenY = try XCTUnwrap(harness.screenY(ofItem: first))
    XCTAssertEqual(screenY, list.rectForItem(at: first).minY - 1204, accuracy: 0.5)

    // Far enough for new rows: one pass mounts them right away, and they cover the viewport.
    list.contentOffset = CGPoint(x: 0, y: 1500)
    XCTAssertFalse(list.needsLayout)
    list.layoutSubtreeIfNeeded()
    XCTAssertEqual(list.shadowListKit_layoutPassCount, passes + 1)
    XCTAssertEqual(list.shadowListKit_mountPassCount, mounts + 1)
    let range = list.visibleRange
    for index in range.location..<NSMaxRange(range) {
      let y = try XCTUnwrap(harness.screenY(ofItem: index))
      XCTAssertEqual(y, list.rectForItem(at: index).minY - 1500, accuracy: 0.5)
    }
    let last = NSMaxRange(range) - 1
    XCTAssertGreaterThanOrEqual(list.rectForItem(at: last).maxY - 1500, list.bounds.height - 0.5)
  }

  /*
   * A list that is not driven runs a pass for every move, as before.
   */
  func testAListThatIsNotDrivenLaysOutEveryMove() {
    let harness = ListHarness(source: SizedTestSource(count: 600))
    let list = harness.list
    list.contentOffset = CGPoint(x: 0, y: 1200)
    harness.layout()
    let passes = list.shadowListKit_layoutPassCount
    list.contentOffset = CGPoint(x: 0, y: 1201)
    XCTAssertTrue(list.needsLayout)
    list.layoutSubtreeIfNeeded()
    XCTAssertEqual(list.shadowListKit_layoutPassCount, passes + 1)
    XCTAssertFalse(list.shadowListKit_viewsBehind)
  }

  /*
   * While the host moves it, the list moves the content and its cells by their layers, which
   * changes no view geometry. Once it rests the views take the layers' geometry.
   */
  func testTheViewsCatchUpWithTheLayersOnceTheListRests() throws {
    let harness = makeDriven()
    let list = harness.list
    let content = try XCTUnwrap(list.subviews.first)
    harness.rest()
    XCTAssertFalse(list.shadowListKit_viewsBehind)
    let origin = content.bounds.origin
    drag(harness, to: 900, step: 60)
    XCTAssertTrue(list.shadowListKit_viewsBehind)
    XCTAssertEqual(content.bounds.origin, origin)
    let transform = try XCTUnwrap(content.layer?.sublayerTransform)
    XCTAssertEqual(transform.m42, origin.y - 900, accuracy: 0.01)
    // Cells mounted on the way sit where their rows are, by their layers.
    let cells = content.subviews.compactMap { $0 as? TestCell }.filter { $0.layer?.isHidden == false }
    XCTAssertFalse(cells.isEmpty)
    for cell in cells {
      let index = try XCTUnwrap(harness.source.keys.firstIndex(of: cell.key))
      XCTAssertEqual(try XCTUnwrap(cell.layer).position.y, list.rectForItem(at: index).minY, accuracy: 0.5)
    }

    harness.rest()
    XCTAssertFalse(list.shadowListKit_viewsBehind)
    XCTAssertEqual(content.bounds.origin.y, 900, accuracy: 0.01)
    XCTAssertTrue(CATransform3DIsIdentity(try XCTUnwrap(content.layer?.sublayerTransform)))
    for cell in content.subviews.compactMap({ $0 as? TestCell }) {
      XCTAssertEqual(cell.isHidden, cell.layer?.isHidden)
      XCTAssertEqual(cell.frame.minY, try XCTUnwrap(cell.layer).position.y, accuracy: 0.01)
    }
    let first = list.visibleRange.location
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: first)), list.rectForItem(at: first).minY - 900, accuracy: 0.5)
  }

  /*
   * AppKit draws no hidden view. A cell recycled while views placed the cells, or a new one, is
   * shown by its view when a driven list mounts it, so it draws its new row while the views lag.
   */
  func testACellShownWhileTheViewsLagHasItsViewShown() throws {
    let source = SizedTestSource(count: 2000)
    let harness = ListHarness(source: source) {
      $0.estimatedItemSize = 30
      $0.allowsSelection = false
    }
    let list = harness.list
    // Not driven yet: cells go back to the pool with their views hidden.
    list.contentOffset = CGPoint(x: 0, y: 1000)
    harness.layout()
    list.contentOffset = .zero
    harness.layout()
    list.isScrollDriven = true
    harness.layout()
    harness.rest()
    let content = try XCTUnwrap(list.subviews.first)
    var y: CGFloat = 0
    for _ in 0..<5 {
      y += 200
      list.contentOffset = CGPoint(x: 0, y: y)
      harness.layout()
    }
    XCTAssertTrue(list.shadowListKit_viewsBehind)
    let shown = content.subviews.compactMap { $0 as? TestCell }.filter { $0.layer?.isHidden == false }
    XCTAssertFalse(shown.isEmpty)
    for cell in shown {
      XCTAssertFalse(cell.isHidden, "row \(cell.key) shows its layer under a hidden view")
    }
  }

  /*
   * AppKit writes a view's frame and hidden state into its layer when it syncs them. A cell the
   * list moved by its layer stays where the list put it.
   */
  func testACellPlacedByItsLayerKeepsItsPlaceThroughAppKitWrites() throws {
    let harness = makeDriven()
    let list = harness.list
    let content = try XCTUnwrap(list.subviews.first)
    drag(harness, to: 900, step: 60)
    let moved = content.subviews.compactMap { $0 as? TestCell }.first {
      $0.layer?.isHidden == false && abs($0.frame.minY - ($0.layer?.position.y ?? 0)) > 1
    }
    let cell = try XCTUnwrap(moved)
    let layer = try XCTUnwrap(cell.layer)
    let position = layer.position
    layer.position = cell.frame.origin
    layer.isHidden = true
    XCTAssertEqual(layer.position, position)
    XCTAssertFalse(layer.isHidden)
  }

  /*
   * A click while the views lag lands on the cell the layers show under it.
   */
  func testAHitTestFindsTheCellWhereItsLayerShows() throws {
    let harness = makeDriven()
    let list = harness.list
    drag(harness, to: 1300, step: 50)
    XCTAssertTrue(list.shadowListKit_viewsBehind)
    let index = list.visibleRange.location + 3
    let rect = list.rectForItem(at: index)
    let point = list.convert(NSPoint(x: 40, y: rect.midY - 1300), to: list.superview)
    let hit = try XCTUnwrap(list.hitTest(point) as? TestCell)
    XCTAssertEqual(hit.key, harness.source.keys[index])
    XCTAssertTrue(list.shadowListKit_viewsBehind)
  }
}
