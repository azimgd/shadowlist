import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Selection with the mouse and the keyboard, the way NSTableView selects.
 */
@MainActor
final class SelectionTests: XCTestCase {
  private final class KeyCatcher: NSView {
    var keys = 0
    var mouseDowns = 0

    override func keyDown(with event: NSEvent) { keys += 1 }
    override func mouseDown(with event: NSEvent) { mouseDowns += 1 }
  }

  private func mouseEvent(_ type: NSEvent.EventType, in list: ShadowListKitListView, at point: CGPoint) -> NSEvent {
    NSEvent.mouseEvent(
      with: type, location: list.convert(point, to: nil), modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime,
      windowNumber: list.window?.windowNumber ?? 0, context: nil, eventNumber: 0, clickCount: 1, pressure: 1)!
  }

  func testClickSelectsOnMouseDownAndHighlightsDuringThePress() throws {
    let harness = SelectionHarness()
    let list = harness.list
    let cell = try XCTUnwrap(harness.cell(2))
    let point = list.convert(NSPoint(x: cell.frame.midX, y: cell.frame.midY), from: cell.superview)
    // The cell passes the press up to the list.
    cell.mouseDown(with: mouseEvent(.leftMouseDown, in: list, at: point))
    XCTAssertEqual(harness.selection, [2])
    XCTAssertTrue(cell.isSelected)
    XCTAssertTrue(cell.isHighlighted)
    XCTAssertEqual(list.clickedIndex, 2)
    XCTAssertTrue(harness.window.firstResponder === list)
    cell.mouseUp(with: mouseEvent(.leftMouseUp, in: list, at: point))
    XCTAssertFalse(cell.isHighlighted)
    XCTAssertEqual(cell.highlights, [true, false])
    XCTAssertEqual(list.clickedIndex, NSNotFound)
    XCTAssertEqual(harness.recorder.selected, [2])
    XCTAssertTrue(list.acceptsFirstMouse(for: nil))
    XCTAssertTrue(cell.acceptsFirstMouse(for: nil))

    // A plain click moves the selection, a click on the selected item changes nothing.
    harness.click(4)
    harness.click(4)
    XCTAssertEqual(harness.selection, [4])
    XCTAssertEqual(harness.recorder.selected, [2, 4])
    XCTAssertEqual(harness.recorder.deselected, [2])
    XCTAssertFalse(cell.isSelected)
  }

  func testCommandClickTogglesAndShiftClickSelectsFromTheAnchor() {
    let harness = SelectionHarness { $0.allowsMultipleSelection = true }
    harness.click(2)
    harness.click(5, .command)
    XCTAssertEqual(harness.selection, [2, 5])
    harness.click(8, .shift)
    XCTAssertEqual(harness.selection, [2, 5, 6, 7, 8])
    // A shorter range from the same anchor gives back the rows it leaves.
    harness.click(6, .shift)
    XCTAssertEqual(harness.selection, [2, 5, 6])
    harness.click(3, .shift)
    XCTAssertEqual(harness.selection, [2, 3, 4, 5])
    harness.click(5, .command)
    XCTAssertEqual(harness.selection, [2, 3, 4])
    XCTAssertEqual(harness.recorder.deselected.sorted(), [5, 6, 7, 8])
    // A refused item stays out of a range.
    harness.recorder.refused = [11]
    harness.click(10)
    harness.click(12, .shift)
    XCTAssertEqual(harness.selection, [10, 12])
    // A plain click leaves one.
    harness.click(1)
    XCTAssertEqual(harness.selection, [1])
  }

  func testSingleSelectionCommandClickDeselectsAndShiftClickSelectsOne() {
    let harness = SelectionHarness()
    harness.click(2)
    harness.click(5, .shift)
    XCTAssertEqual(harness.selection, [5])
    harness.click(5, .command)
    XCTAssertEqual(harness.selection, [])
    harness.click(3, .command)
    XCTAssertEqual(harness.selection, [3])
  }

  func testClickOnNoItemDeselectsAll() {
    let harness = SelectionHarness(count: 3)
    harness.click(1)
    let list = harness.list
    list.mouseDown(with: mouseEvent(.leftMouseDown, in: list, at: CGPoint(x: 100, y: 400)))
    list.mouseUp(with: mouseEvent(.leftMouseUp, in: list, at: CGPoint(x: 100, y: 400)))
    XCTAssertEqual(harness.selection, [])
    XCTAssertEqual(harness.recorder.deselected, [1])
  }

  func testArrowsMoveAndShiftArrowsExtendFromTheAnchor() {
    let harness = SelectionHarness { $0.allowsMultipleSelection = true }
    harness.press(.down)
    XCTAssertEqual(harness.selection, [0])
    harness.click(3)
    harness.press(.down)
    harness.press(.down)
    XCTAssertEqual(harness.selection, [5])
    harness.press(.down, .shift)
    harness.press(.down, .shift)
    XCTAssertEqual(harness.selection, [5, 6, 7])
    harness.press(.up, .shift)
    harness.press(.up, .shift)
    harness.press(.up, .shift)
    XCTAssertEqual(harness.selection, [4, 5])
    harness.press(.up)
    XCTAssertEqual(harness.selection, [3])
    // Left and right step through a grid's columns only.
    harness.press(.right)
    XCTAssertEqual(harness.selection, [3])
  }

  func testArrowsSkipRefusedItemsAndStartFromTheEnd() {
    let harness = SelectionHarness(count: 20)
    harness.recorder.refused = [18, 19]
    harness.press(.up)
    XCTAssertEqual(harness.selection, [17])
    harness.recorder.refused = [16]
    harness.press(.up)
    XCTAssertEqual(harness.selection, [15])
  }

  func testArrowsInAGridMoveByRowsAndColumns() {
    let harness = SelectionHarness { $0.numberOfColumns = 3 }
    harness.click(4)
    harness.press(.down)
    XCTAssertEqual(harness.selection, [7])
    harness.press(.right)
    XCTAssertEqual(harness.selection, [8])
    harness.press(.up)
    XCTAssertEqual(harness.selection, [5])
  }

  func testCommandArrowsGoToTheEndsAndRevealTheItem() {
    let harness = SelectionHarness { $0.allowsMultipleSelection = true }
    harness.click(1)
    harness.press(.down, .command)
    XCTAssertEqual(harness.selection, [599])
    XCTAssertTrue(harness.isFullyVisible(599))
    // At the nearest edge: the last item ends at the viewport's end.
    let list = harness.list
    XCTAssertEqual(list.contentOffset.y, list.contentSize.height - list.bounds.height, accuracy: 0.5)
    harness.press(.up, .command)
    XCTAssertEqual(harness.selection, [0])
    XCTAssertEqual(list.contentOffset.y, 0, accuracy: 0.5)
    harness.press(.down, [.command, .shift])
    XCTAssertEqual(harness.selection.count, 600)
  }

  func testArrowRevealsTheItemAtTheNearestEdge() {
    let harness = SelectionHarness()
    let list = harness.list
    harness.click(9)
    harness.press(.down)
    harness.press(.down)
    XCTAssertEqual(harness.selection, [11])
    XCTAssertTrue(harness.isFullyVisible(11))
    // Item 11 ends at 528: its end rests on the viewport's end.
    XCTAssertEqual(list.contentOffset.y, 528 - 480, accuracy: 0.5)
    list.contentOffset = CGPoint(x: 0, y: 2000)
    harness.layout()
    harness.press(.up)
    XCTAssertEqual(harness.selection, [10])
    XCTAssertEqual(list.contentOffset.y, 440, accuracy: 0.5)
  }

  func testHomeEndAndPageKeysScrollWithoutSelecting() {
    let harness = SelectionHarness()
    let list = harness.list
    harness.click(3)
    harness.press(.pageDown)
    list.shadowListKit_runScrollFrames(for: 1)
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, 480, accuracy: 0.5)
    harness.press(.end)
    list.shadowListKit_runScrollFrames(for: 1)
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, list.contentSize.height - list.bounds.height, accuracy: 0.5)
    harness.press(.pageUp)
    list.shadowListKit_runScrollFrames(for: 1)
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, list.contentSize.height - list.bounds.height - 480, accuracy: 0.5)
    harness.press(.home)
    list.shadowListKit_runScrollFrames(for: 1)
    harness.layout()
    XCTAssertEqual(list.contentOffset.y, 0, accuracy: 0.5)
    XCTAssertEqual(harness.selection, [3])
  }

  func testOptionPageKeysMoveTheSelectionAPage() {
    let harness = SelectionHarness { $0.allowsMultipleSelection = true }
    harness.click(0)
    // Item 10 starts 440 in, the last start within a 480 point page.
    harness.press(.pageDown, .option)
    XCTAssertEqual(harness.selection, [10])
    XCTAssertTrue(harness.isFullyVisible(10))
    harness.press(.pageDown, .shift)
    XCTAssertEqual(harness.selection, Array(10...20))
    harness.press(.pageUp, .option)
    XCTAssertEqual(harness.selection, [10])
  }

  func testReturnSpaceAndDoubleClickActivate() {
    let harness = SelectionHarness()
    harness.press(.returnKey)
    XCTAssertEqual(harness.recorder.activated, [])
    harness.click(4)
    harness.press(.returnKey)
    harness.press(.space)
    XCTAssertEqual(harness.recorder.activated, [4, 4])
    harness.click(6)
    harness.click(6, count: 2)
    XCTAssertEqual(harness.recorder.activated, [4, 4, 6])
    // The double-click's item is clickedIndex while it activates.
    XCTAssertEqual(harness.recorder.clickedWhenActivated.last, 6)
    XCTAssertEqual(harness.selection, [6])
    // A double-click with Command does not activate.
    harness.click(7, .command, count: 2)
    XCTAssertEqual(harness.recorder.activated, [4, 4, 6])
  }

  func testActivateTakesTheKeyboardItemOrTheFirstSelected() {
    let harness = SelectionHarness { $0.allowsMultipleSelection = true }
    harness.click(5)
    harness.click(2, .command)
    harness.press(.returnKey)
    XCTAssertEqual(harness.recorder.activated, [2])
    // The keyboard's item was deselected: the first selected one opens.
    harness.click(2, .command)
    harness.click(8, .command)
    harness.click(8, .command)
    harness.press(.returnKey)
    XCTAssertEqual(harness.recorder.activated, [2, 5])
  }

  func testTypeSelectMatchesPrefixesFromTheKeyboardItemIncludingUnmountedItems() {
    let harness = SelectionHarness()
    let list = harness.list
    let names = ["apple", "Banana", "blueberry", "cherry", "Bean"]
    for (index, name) in names.enumerated() {
      harness.recorder.names[index] = name
    }
    harness.recorder.names[500] = "Élan"
    harness.press(.text("b"))
    XCTAssertEqual(harness.selection, [1])
    harness.press(.text("l"))
    XCTAssertEqual(harness.selection, [2])
    // Unmounted, found through the delegate, without case or diacritics, and scrolled to.
    harness.press(.escape)
    sendKey("e", to: list, at: ProcessInfo.processInfo.systemUptime + 5)
    XCTAssertEqual(harness.selection, [500])
    XCTAssertTrue(harness.isFullyVisible(500))
  }

  func testTypeSelectRestartsAfterASecondAndCyclesOnARepeatedCharacter() {
    let harness = SelectionHarness()
    let list = harness.list
    for (index, name) in ["apple", "banana", "bean", "berry", "cherry"].enumerated() {
      harness.recorder.names[index] = name
    }
    let start = ProcessInfo.processInfo.systemUptime
    sendKey("b", to: list, at: start)
    sendKey("b", to: list, at: start + 0.2)
    sendKey("b", to: list, at: start + 0.4)
    XCTAssertEqual(harness.selection, [3])
    // Past a second the buffer starts again.
    sendKey("c", to: list, at: start + 2)
    XCTAssertEqual(harness.selection, [4])
    sendKey("b", to: list, at: start + 4)
    sendKey("e", to: list, at: start + 4.5)
    sendKey("r", to: list, at: start + 5)
    XCTAssertEqual(harness.selection, [3])
    // A Space inside a prefix types, outside it activates.
    XCTAssertEqual(harness.recorder.activated, [])
    sendKey(" ", to: list, at: start + 7)
    XCTAssertEqual(harness.recorder.activated, [3])
  }

  private func sendKey(_ characters: String, to list: ShadowListKitListView, at timestamp: TimeInterval) {
    let event = NSEvent.keyEvent(
      with: .keyDown, location: .zero, modifierFlags: [], timestamp: timestamp, windowNumber: list.window?.windowNumber ?? 0,
      context: nil, characters: characters, charactersIgnoringModifiers: characters, isARepeat: false, keyCode: 0)!
    list.keyDown(with: event)
    list.layoutSubtreeIfNeeded()
  }

  func testSelectAllNeedsMultipleSelectionAndEscapeDeselects() {
    let harness = SelectionHarness(count: 30)
    harness.click(2)
    harness.list.selectAll(nil)
    XCTAssertEqual(harness.selection, [2])
    harness.list.allowsMultipleSelection = true
    harness.recorder.refused = [7]
    harness.list.selectAll(nil)
    XCTAssertEqual(harness.selection.count, 29)
    XCTAssertFalse(harness.selection.contains(7))
    XCTAssertEqual(harness.recorder.selected.count, 29)
    harness.press(.escape)
    XCTAssertEqual(harness.selection, [])
    XCTAssertEqual(harness.recorder.deselected.count, 29)
  }

  func testSelectedCellsShowTheirStateWhenMountedAgain() {
    let harness = SelectionHarness()
    let list = harness.list
    harness.click(1)
    list.contentOffset = CGPoint(x: 0, y: 8000)
    harness.layout()
    XCTAssertNil(list.cellForItem(at: 1))
    list.contentOffset = .zero
    harness.layout()
    XCTAssertEqual(harness.cell(1)?.isSelected, true)
    XCTAssertEqual(harness.cell(2)?.isSelected, false)
    // The selection follows its key across an insert above.
    harness.source.keys.insert("new", at: 0)
    list.insertItems(at: IndexSet(integer: 0))
    harness.layout()
    XCTAssertEqual(harness.selection, [2])
    harness.press(.down)
    XCTAssertEqual(harness.selection, [3])
  }

  func testDeletedItemsLeaveTheSelectionAndTheAnchor() {
    let harness = SelectionHarness(count: 30) { $0.allowsMultipleSelection = true }
    let list = harness.list
    harness.click(5)
    harness.click(2, .command)
    harness.source.keys.remove(at: 2)
    list.deleteItems(at: IndexSet(integer: 2))
    harness.layout()
    XCTAssertEqual(harness.selection, [4])
    XCTAssertEqual(list.accessibilitySelectedRows()?.count, 1)
    // The same key coming back is a new item, not selected.
    harness.source.keys.insert("item-2", at: 2)
    list.insertItems(at: IndexSet(integer: 2))
    harness.layout()
    XCTAssertEqual(harness.selection, [5])
    // The anchor left with its item. A Shift-click selects one item.
    harness.click(8, .shift)
    XCTAssertEqual(harness.selection, [8])
  }

  func testEmphasisFollowsTheFirstResponderAndTheKeyWindow() throws {
    let harness = SelectionHarness()
    let list = harness.list
    let window = harness.window
    harness.click(1)
    let selected = try XCTUnwrap(harness.cell(1))
    let other = try XCTUnwrap(harness.cell(3))
    XCTAssertTrue(window.firstResponder === list)
    XCTAssertFalse(selected.isEmphasized)
    window.key = true
    NotificationCenter.default.post(name: NSWindow.didBecomeKeyNotification, object: window)
    XCTAssertTrue(selected.isEmphasized)
    XCTAssertTrue(other.isEmphasized)
    window.makeFirstResponder(nil)
    XCTAssertFalse(selected.isEmphasized)
    window.makeFirstResponder(list)
    XCTAssertTrue(selected.isEmphasized)
    window.key = false
    NotificationCenter.default.post(name: NSWindow.didResignKeyNotification, object: window)
    XCTAssertFalse(selected.isEmphasized)
    window.key = true
    NotificationCenter.default.post(name: NSWindow.didBecomeKeyNotification, object: window)
    // A cell mounted while emphasized comes emphasized.
    list.contentOffset = CGPoint(x: 0, y: 4000)
    harness.layout()
    XCTAssertTrue(list.visibleCells.allSatisfy { $0.isEmphasized })
    // The focus ring follows focusRingType.
    XCTAssertEqual(list.focusRingMaskBounds, list.bounds)
    list.focusRingType = .none
    XCTAssertEqual(list.focusRingMaskBounds, .zero)
  }

  func testWithoutSelectionTheListLeavesEventsToTheNextResponder() throws {
    let harness = SelectionHarness { $0.allowsSelection = false }
    let list = harness.list
    let catcher = KeyCatcher(frame: harness.window.contentView!.bounds)
    harness.window.contentView = catcher
    catcher.addSubview(list)
    harness.layout()
    XCTAssertFalse(list.acceptsFirstResponder)
    XCTAssertFalse(list.acceptsFirstMouse(for: nil))
    harness.window.makeFirstResponder(list)
    XCTAssertFalse(harness.window.firstResponder === list)
    harness.click(2)
    harness.press(.down)
    harness.press(.text("a"))
    XCTAssertEqual(harness.selection, [])
    XCTAssertEqual(catcher.keys, 2)
    XCTAssertEqual(catcher.mouseDowns, 1)
    XCTAssertTrue(list.trackingAreas.isEmpty)
    list.selectItem(at: 1, animated: false)
    XCTAssertEqual(harness.selection, [])

    // Turning selection off takes the selection, the first responder and the tracking.
    list.allowsSelection = true
    XCTAssertEqual(list.trackingAreas.count, 1)
    harness.click(2)
    XCTAssertTrue(harness.window.firstResponder === list)
    list.allowsSelection = false
    XCTAssertEqual(harness.selection, [])
    XCTAssertFalse(harness.window.firstResponder === list)
    XCTAssertTrue(list.trackingAreas.isEmpty)
    XCTAssertEqual(harness.cell(2)?.isSelected, false)
  }

  func testProgrammaticSelectionSetsTheKeyboardItem() {
    let harness = SelectionHarness()
    harness.list.selectItem(at: 6, animated: false)
    XCTAssertEqual(harness.cell(6)?.isSelected, true)
    XCTAssertEqual(harness.recorder.selected, [])
    harness.press(.down)
    XCTAssertEqual(harness.selection, [7])
    harness.list.deselectItem(at: 7, animated: false)
    XCTAssertEqual(harness.selection, [])
  }
}
