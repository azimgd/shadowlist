import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Drag to reorder with the mouse: the lift past the slop, the drop spot, the edge scroll, the
 * drop and Escape.
 */
@MainActor
final class DragTests: XCTestCase {
  /*
   * Moves the keys of its source the way a host moves its data, and applies a driven list's
   * offsets.
   */
  private final class MoveRecorder: NSObject, ShadowListKitListViewDelegate {
    let source: TestSource
    var moves: [[Int]] = []
    var fixed: Set<Int> = []
    var corrections = 0

    init(source: TestSource) {
      self.source = source
    }

    func listView(_ listView: ShadowListKitListView, canMoveItemAt index: Int) -> Bool { !fixed.contains(index) }

    func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
      moves.append([sourceIndex, destinationIndex])
      let key = source.keys.remove(at: sourceIndex)
      source.keys.insert(key, at: destinationIndex)
    }

    func listView(_ listView: ShadowListKitListView, didCorrectOffset contentOffset: CGPoint) {
      corrections += 1
      listView.contentOffset = contentOffset
    }
  }

  /*
   * Items of 44 points under a recorder, reorderable.
   */
  private func makeHarness(
    count: Int = 600, configure: @escaping (ShadowListKitListView) -> Void = { _ in }
  ) -> (ListHarness, MoveRecorder) {
    let source = SizedTestSource(count: count)
    let harness = ListHarness(source: source) {
      $0.reorderEnabled = true
      configure($0)
    }
    let recorder = MoveRecorder(source: source)
    harness.list.delegate = recorder
    return (harness, recorder)
  }

  /*
   * The middle of an item's cell in the list's coordinates.
   */
  private func center(_ list: ShadowListKitListView, _ index: Int) throws -> CGPoint {
    let cell = try XCTUnwrap(list.cellForItem(at: index))
    let frame = list.convert(cell.frame, from: cell.superview)
    return CGPoint(x: frame.midX, y: frame.midY)
  }

  /*
   * Where a cell's layer shows, its transform included, in the list's coordinates.
   */
  private func shownFrame(_ list: ShadowListKitListView, _ cell: ShadowListKitListCell) throws -> CGRect {
    let layer = try XCTUnwrap(cell.layer)
    let offset = list.contentOffset
    return layer.frame.offsetBy(dx: -offset.x, dy: -offset.y)
  }

  private func moved(_ point: CGPoint, x: CGFloat = 0, y: CGFloat = 0) -> CGPoint {
    CGPoint(x: point.x + x, y: point.y + y)
  }

  func testTheRowLiftsOnlyOnceTheMouseDragsPastTheSlop() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 2))
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
    list.shadowListKit_mouseDrag(to: moved(start, y: 4))
    XCTAssertEqual(list.shadowListKit_heldIndex, 2)
    let cell = try XCTUnwrap(list.cellForItem(at: 2))
    XCTAssertGreaterThan(cell.layer?.shadowOpacity ?? 0, 0)
    // The row follows the mouse from where it was pressed, lifted a little bigger.
    let shown = try shownFrame(list, cell)
    XCTAssertEqual(shown.midY, start.y + 4, accuracy: 0.5)
    XCTAssertGreaterThan(shown.height, 44)
    list.shadowListKit_mouseUp(at: moved(start, y: 4))
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
  }

  func testAPressBelowTheSlopIsAClick() throws {
    let (harness, recorder) = makeHarness()
    let list = harness.list
    let start = try center(list, 3)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, x: 2, y: -2))
    XCTAssertTrue(list.cellForItem(at: 3)?.isHighlighted == true)
    list.shadowListKit_mouseUp(at: moved(start, x: 2, y: -2))
    XCTAssertEqual(Array(list.selectedIndices), [3])
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
    XCTAssertTrue(recorder.moves.isEmpty)
    XCTAssertEqual(list.clickedIndex, NSNotFound)
  }

  func testALiftTakesThePressHighlightAndTheHoverOff() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let start = try center(list, 1)
    list.shadowListKit_moveMouse(to: start)
    XCTAssertTrue(list.cellForItem(at: 1)?.isHovered == true)
    list.shadowListKit_mouseDown(at: start)
    XCTAssertTrue(list.cellForItem(at: 1)?.isHighlighted == true)
    list.shadowListKit_mouseDrag(to: moved(start, y: 30))
    XCTAssertFalse(list.cellForItem(at: 1)?.isHighlighted == true)
    XCTAssertTrue(list.visibleCells.allSatisfy { !$0.isHovered })
    list.shadowListKit_mouseUp(at: moved(start, y: 30))
  }

  func testTheDropSpotFollowsTheMouseAndTheOtherRowsMakeRoom() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 10))
    XCTAssertEqual(list.shadowListKit_dropIndex, 2)
    // Past the middle of items 3, 4 and 5.
    list.shadowListKit_mouseDrag(to: moved(start, y: 140))
    XCTAssertEqual(list.shadowListKit_dropIndex, 5)
    for index in 3...5 {
      let cell = try XCTUnwrap(list.cellForItem(at: index))
      XCTAssertEqual(cell.layer?.transform.m42 ?? 0, -44, accuracy: 0.01, "item \(index)")
    }
    XCTAssertTrue(CATransform3DIsIdentity(try XCTUnwrap(list.cellForItem(at: 6)?.layer?.transform)))
    // Back up to the start, past items 1 and 0.
    list.shadowListKit_mouseDrag(to: moved(start, y: -100))
    XCTAssertEqual(list.shadowListKit_dropIndex, 0)
    XCTAssertEqual(list.cellForItem(at: 1)?.layer?.transform.m42 ?? 0, 44, accuracy: 0.01)
    XCTAssertTrue(CATransform3DIsIdentity(try XCTUnwrap(list.cellForItem(at: 4)?.layer?.transform)))
    list.cancelOperation(nil)
  }

  func testTheDropMovesTheItemThroughTheDelegate() throws {
    let (harness, recorder) = makeHarness()
    let list = harness.list
    let keys = harness.source.keys
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 140))
    let offset = list.contentOffset
    list.shadowListKit_mouseUp(at: moved(start, y: 140))
    harness.layout()
    XCTAssertEqual(recorder.moves, [[2, 5]])
    XCTAssertEqual(harness.source.keys[5], keys[2])
    XCTAssertEqual(list.contentOffset, offset)
    // Every row rests in its new slot.
    for index in 0..<8 {
      let cell = try XCTUnwrap(list.cellForItem(at: index) as? TestCell)
      XCTAssertEqual(cell.key, harness.source.keys[index])
      XCTAssertEqual(harness.screenY(ofItem: index) ?? -1, CGFloat(index) * 44, accuracy: 0.5)
      XCTAssertTrue(CATransform3DIsIdentity(try XCTUnwrap(cell.layer?.transform)), "item \(index)")
    }
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
  }

  /*
   * The dropped row slides once, from the lifted place it was let go at to its slot.
   */
  func testTheDroppedRowSlidesHomeFromWhereItWasLetGo() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 30))
    let cell = try XCTUnwrap(list.cellForItem(at: 2))
    let held = try XCTUnwrap(cell.layer?.transform)
    list.shadowListKit_mouseUp(at: moved(start, y: 30))
    let slide = try XCTUnwrap(cell.layer?.animation(forKey: "transform") as? CABasicAnimation)
    let from = try XCTUnwrap((slide.fromValue as? NSValue)?.caTransform3DValue)
    XCTAssertTrue(CATransform3DEqualToTransform(from, held))
    XCTAssertTrue(CATransform3DIsIdentity(try XCTUnwrap(cell.layer?.transform)))
  }

  func testADropInPlaceMovesNothing() throws {
    let (harness, recorder) = makeHarness()
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 12))
    list.shadowListKit_mouseUp(at: moved(start, y: 12))
    XCTAssertTrue(recorder.moves.isEmpty)
  }

  func testEscapePutsTheRowBack() throws {
    let (harness, recorder) = makeHarness()
    let list = harness.list
    let keys = harness.source.keys
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 140))
    XCTAssertEqual(Array(list.selectedIndices), [2])
    list.shadowListKit_pressKey("\u{1B}", keyCode: 53, modifierFlags: [])
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
    // Escape ended the drag, not the selection.
    XCTAssertEqual(Array(list.selectedIndices), [2])
    list.shadowListKit_mouseDrag(to: moved(start, y: 200))
    list.shadowListKit_mouseUp(at: moved(start, y: 200))
    harness.layout()
    XCTAssertTrue(recorder.moves.isEmpty)
    XCTAssertEqual(harness.source.keys, keys)
    for index in 0..<8 {
      let cell = try XCTUnwrap(list.cellForItem(at: index))
      XCTAssertTrue(CATransform3DIsIdentity(try XCTUnwrap(cell.layer?.transform)), "item \(index)")
    }
  }

  func testTheListScrollsNearTheEdgeAndKeepsTheRowUnderTheMouse() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    let edge = CGPoint(x: start.x, y: list.bounds.height - 10)
    list.shadowListKit_mouseDrag(to: edge)
    list.shadowListKit_runDragFrames(for: 0.5)
    let travel = list.contentOffset.y
    XCTAssertGreaterThan(travel, 100)
    XCTAssertLessThanOrEqual(travel, 960 * 0.5 + 1)
    let cell = try XCTUnwrap(list.cellForItem(at: 2))
    XCTAssertEqual(try shownFrame(list, cell).midY, edge.y, accuracy: 0.5)
    // Away from the edges it stops.
    list.shadowListKit_mouseDrag(to: CGPoint(x: start.x, y: list.bounds.height / 2))
    let resting = list.contentOffset.y
    list.shadowListKit_runDragFrames(for: 0.2)
    XCTAssertEqual(list.contentOffset.y, resting)
    // Toward the start.
    list.shadowListKit_mouseDrag(to: CGPoint(x: start.x, y: 5))
    list.shadowListKit_runDragFrames(for: 0.2)
    XCTAssertLessThan(list.contentOffset.y, resting)
    list.cancelOperation(nil)
  }

  func testTheHeldCellStaysMountedWhileTheListScrollsFarAway() throws {
    let (harness, recorder) = makeHarness()
    let list = harness.list
    let held = try XCTUnwrap(list.cellForItem(at: 2) as? TestCell)
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    let edge = CGPoint(x: start.x, y: list.bounds.height - 2)
    list.shadowListKit_mouseDrag(to: edge)
    list.shadowListKit_runDragFrames(for: 4)
    XCTAssertGreaterThan(list.contentOffset.y, 2000)
    XCTAssertEqual(list.shadowListKit_heldIndex, 2)
    XCTAssertTrue(list.cellForItem(at: 2) === held)
    XCTAssertFalse(held.isHidden)
    XCTAssertEqual(held.key, harness.source.keys[2])
    XCTAssertEqual(try shownFrame(list, held).midY, edge.y, accuracy: 0.5)
    let drop = list.shadowListKit_dropIndex
    XCTAssertGreaterThan(drop, 40)
    list.shadowListKit_mouseUp(at: edge)
    XCTAssertEqual(recorder.moves, [[2, drop]])
  }

  func testReorderWorksWithoutSelection() throws {
    let (harness, recorder) = makeHarness { $0.allowsSelection = false }
    let list = harness.list
    let start = try center(list, 0)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 60))
    XCTAssertEqual(list.shadowListKit_heldIndex, 0)
    list.cancelOperation(nil)
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
    list.shadowListKit_mouseUp(at: moved(start, y: 60))
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 60))
    list.shadowListKit_mouseUp(at: moved(start, y: 60))
    XCTAssertEqual(recorder.moves, [[0, 1]])
    XCTAssertTrue(list.selectedIndices.isEmpty)
  }

  func testItemsTheDelegateRefusesAndModifiedPressesDoNotLift() throws {
    let (harness, recorder) = makeHarness()
    recorder.fixed = [1]
    let list = harness.list
    let start = try center(list, 1)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 60))
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
    list.shadowListKit_mouseUp(at: moved(start, y: 60))
    list.shadowListKit_clickItem(at: 3, modifierFlags: .command, clickCount: 1)
    list.reorderEnabled = false
    let other = try center(list, 4)
    list.shadowListKit_mouseDown(at: other)
    list.shadowListKit_mouseDrag(to: moved(other, y: 60))
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
    list.shadowListKit_mouseUp(at: moved(other, y: 60))
    XCTAssertTrue(recorder.moves.isEmpty)
  }

  func testTurningReorderOffPutsAHeldRowBack() throws {
    let (harness, recorder) = makeHarness()
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_beginDrag(at: start)
    list.shadowListKit_moveDrag(to: moved(start, y: 140))
    XCTAssertEqual(list.shadowListKit_dropIndex, 5)
    list.reorderEnabled = false
    XCTAssertEqual(list.shadowListKit_heldIndex, NSNotFound)
    list.shadowListKit_endDrag()
    XCTAssertTrue(recorder.moves.isEmpty)
  }

  func testAHorizontalListReordersSideways() throws {
    let (harness, recorder) = makeHarness {
      $0.isHorizontal = true
    }
    let list = harness.list
    let first = try XCTUnwrap(list.cellForItem(at: 0))
    XCTAssertEqual(first.frame.width, 44)
    let start = try center(list, 1)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, x: 100))
    XCTAssertEqual(list.shadowListKit_dropIndex, 3)
    XCTAssertEqual(list.cellForItem(at: 2)?.layer?.transform.m41 ?? 0, -44, accuracy: 0.01)
    XCTAssertEqual(try shownFrame(list, try XCTUnwrap(list.cellForItem(at: 1))).midX, start.x + 100, accuracy: 0.5)
    list.shadowListKit_mouseUp(at: moved(start, x: 100))
    XCTAssertEqual(recorder.moves, [[1, 3]])
  }

  func testAGridMovesCellsAcrossColumns() throws {
    let (harness, recorder) = makeHarness {
      $0.numberOfColumns = 2
    }
    let list = harness.list
    let start = try center(list, 0)
    list.shadowListKit_mouseDown(at: start)
    // Over item 3, the second row's right cell.
    let target = try center(list, 3)
    list.shadowListKit_mouseDrag(to: target)
    XCTAssertEqual(list.shadowListKit_dropIndex, 3)
    list.shadowListKit_mouseUp(at: target)
    XCTAssertEqual(recorder.moves, [[0, 3]])
  }

  func testInASectionedListAnItemStaysInItsSection() throws {
    final class Sections: TestSource {
      func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 44 }
      func numberOfSections(in listView: ShadowListKitListView) -> Int { 3 }
      func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { keys.count / 3 }
      func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { "Section \(section)" }
      func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat { 30 }
    }
    let source = Sections(count: 12)
    let harness = ListHarness(source: source) { $0.reorderEnabled = true }
    let recorder = MoveRecorder(source: source)
    harness.list.delegate = recorder
    let list = harness.list
    // The last item of the first section, dragged deep into the second.
    let start = try center(list, 3)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 150))
    XCTAssertEqual(list.shadowListKit_dropIndex, 3)
    list.shadowListKit_mouseDrag(to: moved(start, y: -100))
    XCTAssertEqual(list.shadowListKit_dropIndex, 1)
    list.shadowListKit_mouseUp(at: moved(start, y: -100))
    XCTAssertEqual(recorder.moves, [[3, 1]])
  }

  func testADrivenListAsksItsHostToScrollNearTheEdge() throws {
    let (harness, recorder) = makeHarness {
      $0.scrollDriven = true
      $0.allowsSelection = false
    }
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    let edge = CGPoint(x: start.x, y: list.bounds.height - 10)
    list.shadowListKit_mouseDrag(to: edge)
    list.shadowListKit_runDragFrames(for: 0.5)
    XCTAssertGreaterThan(recorder.corrections, 10)
    XCTAssertGreaterThan(list.contentOffset.y, 100)
    let cell = try XCTUnwrap(list.cellForItem(at: 2))
    XCTAssertEqual(try shownFrame(list, cell).midY, edge.y, accuracy: 0.5)
    list.shadowListKit_mouseUp(at: edge)
    XCTAssertEqual(recorder.moves.count, 1)
  }

  func testARecycledCellComesBackWithoutItsDragShift() throws {
    let (harness, _) = makeHarness()
    let list = harness.list
    let start = try center(list, 2)
    list.shadowListKit_mouseDown(at: start)
    list.shadowListKit_mouseDrag(to: moved(start, y: 140))
    let shifted = try XCTUnwrap(list.cellForItem(at: 4))
    XCTAssertFalse(CATransform3DIsIdentity(try XCTUnwrap(shifted.layer?.transform)))
    list.shadowListKit_mouseDrag(to: CGPoint(x: start.x, y: list.bounds.height - 2))
    list.shadowListKit_runDragFrames(for: 3)
    XCTAssertNil(list.cellForItem(at: 4))
    for cell in list.visibleCells where cell.index != 2 {
      XCTAssertEqual(cell.layer?.zPosition ?? 0, 0)
      XCTAssertEqual(cell.layer?.shadowOpacity ?? 0, 0)
    }
    list.cancelOperation(nil)
  }
}
