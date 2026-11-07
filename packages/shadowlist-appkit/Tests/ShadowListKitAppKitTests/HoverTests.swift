import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * The item under the pointer is hovered while the list rests, and again after the content moves
 * under a still pointer.
 */
@MainActor
final class HoverTests: XCTestCase {
  func testThePointerHoversTheItemUnderIt() throws {
    let harness = SelectionHarness()
    let list = harness.list
    list.shadowListKit_moveMouse(to: CGPoint(x: 100, y: 100))
    let second = try XCTUnwrap(harness.cell(2))
    XCTAssertTrue(second.isHovered)
    list.shadowListKit_moveMouse(to: CGPoint(x: 100, y: 200))
    XCTAssertFalse(second.isHovered)
    XCTAssertEqual(harness.cell(4)?.isHovered, true)
    list.shadowListKit_mouseExited()
    XCTAssertTrue(list.visibleCells.allSatisfy { !$0.isHovered })
  }

  func testTheContentMovingUnderAStillPointerHoversTheNewItem() throws {
    let harness = SelectionHarness()
    let list = harness.list
    list.shadowListKit_moveMouse(to: CGPoint(x: 100, y: 100))
    XCTAssertEqual(harness.cell(2)?.isHovered, true)
    list.contentOffset = CGPoint(x: 0, y: 44)
    harness.layout()
    XCTAssertEqual(harness.cell(2)?.isHovered, false)
    XCTAssertEqual(harness.cell(3)?.isHovered, true)
    // A scroll that unmounts the hovered cell hovers the item now under the pointer.
    list.contentOffset = CGPoint(x: 0, y: 8800)
    harness.layout()
    XCTAssertEqual(harness.cell(202)?.isHovered, true)
    XCTAssertEqual(list.visibleCells.filter { $0.isHovered }.count, 1)
  }

  func testDraggingClearsTheHoverUntilTheListRests() throws {
    let harness = SelectionHarness()
    let list = harness.list
    list.shadowListKit_moveMouse(to: CGPoint(x: 100, y: 100))
    list.scrollPhase = .dragging
    XCTAssertEqual(harness.cell(2)?.isHovered, false)
    list.contentOffset = CGPoint(x: 0, y: 88)
    harness.layout()
    XCTAssertTrue(list.visibleCells.allSatisfy { !$0.isHovered })
    list.scrollPhase = .idle
    harness.layout()
    XCTAssertEqual(harness.cell(4)?.isHovered, true)
  }

  func testAPressedItemLosesItsHighlightWhenDraggingBegins() throws {
    let harness = SelectionHarness()
    let list = harness.list
    let cell = try XCTUnwrap(harness.cell(1))
    let point = list.convert(NSPoint(x: cell.frame.midX, y: cell.frame.midY), from: cell.superview)
    list.mouseDown(
      with: NSEvent.mouseEvent(
        with: .leftMouseDown, location: list.convert(point, to: nil), modifierFlags: [], timestamp: 0,
        windowNumber: harness.window.windowNumber, context: nil, eventNumber: 0, clickCount: 1, pressure: 1)!)
    XCTAssertTrue(cell.isHighlighted)
    list.scrollPhase = .dragging
    XCTAssertFalse(cell.isHighlighted)
    list.scrollPhase = .idle
  }

  func testWithoutSelectionNothingIsHovered() {
    let harness = SelectionHarness { $0.allowsSelection = false }
    let list = harness.list
    list.mouseMoved(with: NSEvent.mouseEvent(
      with: .mouseMoved, location: list.convert(CGPoint(x: 100, y: 100), to: nil), modifierFlags: [], timestamp: 0,
      windowNumber: harness.window.windowNumber, context: nil, eventNumber: 0, clickCount: 0, pressure: 0)!)
    XCTAssertTrue(list.visibleCells.allSatisfy { !$0.isHovered })
    list.allowsSelection = true
    list.shadowListKit_moveMouse(to: CGPoint(x: 100, y: 100))
    XCTAssertEqual(harness.cell(2)?.isHovered, true)
    list.allowsSelection = false
    XCTAssertEqual(harness.cell(2)?.isHovered, false)
  }
}
