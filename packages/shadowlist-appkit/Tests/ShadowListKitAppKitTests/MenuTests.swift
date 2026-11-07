import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Context menus from the delegate, with clickedIndex and the outline while one is open.
 */
@MainActor
final class MenuTests: XCTestCase {
  private func closeMenu(_ menu: NSMenu) {
    NotificationCenter.default.post(name: NSMenu.didEndTrackingNotification, object: menu)
  }

  func testARightClickGetsTheDelegatesMenuAndMarksTheItem() throws {
    let harness = SelectionHarness()
    let list = harness.list
    XCTAssertEqual(list.clickedIndex, NSNotFound)
    let menu = try XCTUnwrap(list.shadowListKit_menuForItem(at: 4, modifierFlags: []))
    XCTAssertEqual(menu.title, "item-4")
    XCTAssertEqual(list.clickedIndex, 4)
    let cell = try XCTUnwrap(harness.cell(4))
    XCTAssertTrue(cell.isMenuTarget)
    let outline = try XCTUnwrap(list.shadowListKit_menuOutline)
    XCTAssertTrue(outline.superlayer === cell.layer)
    XCTAssertGreaterThan(outline.borderWidth, 0)
    // The menu does not select.
    XCTAssertEqual(harness.selection, [])

    closeMenu(menu)
    XCTAssertEqual(list.clickedIndex, NSNotFound)
    XCTAssertFalse(cell.isMenuTarget)
    XCTAssertNil(list.shadowListKit_menuOutline)
  }

  func testAControlClickGetsTheSameMenu() throws {
    let harness = SelectionHarness()
    let list = harness.list
    let menu = try XCTUnwrap(list.shadowListKit_menuForItem(at: 2, modifierFlags: .control))
    XCTAssertEqual(menu.title, "item-2")
    XCTAssertEqual(list.clickedIndex, 2)
    // A second menu moves the outline.
    let next = try XCTUnwrap(list.shadowListKit_menuForItem(at: 6, modifierFlags: []))
    XCTAssertEqual(harness.cell(2)?.isMenuTarget, false)
    XCTAssertEqual(harness.cell(6)?.isMenuTarget, true)
    XCTAssertTrue(list.shadowListKit_menuOutline?.superlayer === harness.cell(6)?.layer)
    closeMenu(next)
    XCTAssertEqual(list.clickedIndex, NSNotFound)
  }

  func testItemsWithoutAMenuGetNone() {
    let harness = SelectionHarness()
    let list = harness.list
    XCTAssertNil(list.shadowListKit_menuForItem(at: 3, modifierFlags: []))
    XCTAssertEqual(list.clickedIndex, NSNotFound)
    XCTAssertNil(list.shadowListKit_menuOutline)
    harness.recorder.menus = false
    XCTAssertNil(list.shadowListKit_menuForItem(at: 4, modifierFlags: []))
  }

  func testMenusShowWithoutSelection() throws {
    let harness = SelectionHarness { $0.allowsSelection = false }
    let list = harness.list
    let menu = try XCTUnwrap(list.shadowListKit_menuForItem(at: 0, modifierFlags: []))
    XCTAssertEqual(list.clickedIndex, 0)
    closeMenu(menu)
  }

  func testTheMenuTargetFollowsItsKeyAcrossAnInsert() throws {
    let harness = SelectionHarness()
    let list = harness.list
    let menu = try XCTUnwrap(list.shadowListKit_menuForItem(at: 2, modifierFlags: []))
    harness.source.keys.insert("new", at: 0)
    list.insertItems(at: IndexSet(integer: 0))
    harness.layout()
    XCTAssertEqual(list.clickedIndex, 3)
    closeMenu(menu)
  }

  func testAListWithoutMenusLeavesTheEventToAppKit() {
    let harness = ListHarness(source: TestSource(count: 20))
    XCTAssertNil(harness.list.shadowListKit_menuForItem(at: 1, modifierFlags: []))
  }
}
