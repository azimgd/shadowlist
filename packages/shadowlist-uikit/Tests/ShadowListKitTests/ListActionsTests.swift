import ShadowListKit
import UIKit
import XCTest

@MainActor
final class ListActionsTests: XCTestCase {
  private final class Rows: NSObject, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
    var keys = (0..<20).map { "item-\($0)" }
    var providerKeys: [String] = []
    var actedKeys: [String] = []
    var trailingCalls = 0
    var leadingHook: ((ShadowListKitListView) -> Void)?
    func numberOfItems(in listView: ShadowListKitListView) -> Int { keys.count }
    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }
    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      let cell = listView.dequeueReusableCell(withIdentifier: "row")
      cell.isAccessibilityElement = true
      cell.accessibilityLabel = keys[index]
      return cell
    }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 40 }
    func listView(_ listView: ShadowListKitListView, leadingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
      let key = keys[index]
      providerKeys.append(key)
      let hook = leadingHook
      leadingHook = nil
      hook?(listView)
      let configuration = ShadowListKitSwipeActionsConfiguration(actions: [
        ShadowListKitSwipeAction(style: .normal, title: "Archive \(key)") { [weak self] _, done in
          self?.actedKeys.append(key); done(true)
        },
      ])
      configuration.performsFirstActionWithFullSwipe = false
      return configuration
    }
    func listView(_ listView: ShadowListKitListView, trailingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
      trailingCalls += 1
      return nil
    }
  }

  private func make(_ rows: Rows) -> ShadowListKitListView {
    let list = ShadowListKitListView()
    list.contentInsetAdjustmentBehavior = .never
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.dataSource = rows; list.delegate = rows
    list.reloadData()
    return list
  }

  func testListSwipeAndNativeCustomActionsShareCurrentHandlersWithoutSelecting() throws {
    let rows = Rows()
    let list = make(rows)
    let window = host(list, size: CGSize(width: 300, height: 200))
    defer { window.isHidden = true }
    let cell = try XCTUnwrap(list.cellForItem(at: 0))
    let inspect = UIAccessibilityCustomAction(name: "Inspect") { _ in true }
    cell.accessibilityCustomActions = [inspect]
    let actions = try XCTUnwrap(cell.accessibilityCustomActions)
    XCTAssertEqual(actions.map(\.name), ["Inspect", "Archive item-0"])
    XCTAssertTrue(actions[0] === inspect)
    XCTAssertEqual(actions[1].actionHandler?(actions[1]), true)
    XCTAssertEqual(rows.actedKeys, ["item-0"])
    XCTAssertTrue(list.selectedIndices.isEmpty)
    list.shadowListKit_swipeItem(at: 0, distance: 100, velocity: 0)
    XCTAssertGreaterThan(cell.transform.tx, 0)
    list.closeSwipeActions(animated: false)
    XCTAssertEqual(cell.transform, .identity)
    list.removeFromSuperview()
    XCTAssertEqual(actions[1].actionHandler?(actions[1]), false)
    XCTAssertEqual(cell.accessibilityCustomActions?.map(\.name), ["Inspect"])
  }

  func testEditingKeepsSwipeAndItsAccessibilityActionsDisabled() throws {
    let rows = Rows()
    let list = make(rows)
    let window = host(list, size: CGSize(width: 300, height: 200))
    defer { window.isHidden = true }
    let cell = try XCTUnwrap(list.cellForItem(at: 0))
    list.isEditing = true
    let calls = rows.providerKeys.count
    list.shadowListKit_swipeItem(at: 0, distance: 300, velocity: 1000)
    XCTAssertEqual(cell.transform, .identity)
    XCTAssertTrue(cell.accessibilityCustomActions?.isEmpty ?? true)
    XCTAssertEqual(rows.providerKeys.count, calls)
    XCTAssertTrue(rows.actedKeys.isEmpty)
  }

  func testProviderKeyReloadRejectsTheOldAttachedCellBeforeRemount() throws {
    let rows = Rows()
    let list = make(rows)
    let window = host(list, size: CGSize(width: 300, height: 200))
    defer { window.isHidden = true }
    let old = try XCTUnwrap(list.cellForItem(at: 0))
    rows.leadingHook = { current in
      // The native layout guard defers remounting during an application callback.
      // Keep that existing branch active to isolate key validation before a cell is recycled.
      current.setValue(true, forKey: "inLayout")
      rows.keys[0] = "replacement"
      current.reloadData()
      current.setValue(false, forKey: "inLayout")
    }
    let actions = old.accessibilityCustomActions ?? []
    XCTAssertTrue(old.superview === list)
    XCTAssertEqual(old.index, 0)
    XCTAssertTrue(actions.isEmpty)
    XCTAssertEqual(rows.providerKeys, ["item-0"])
    XCTAssertEqual(rows.trailingCalls, 0)
    XCTAssertTrue(rows.actedKeys.isEmpty)
    list.setNeedsLayout(); list.layoutIfNeeded()
    let replacement = try XCTUnwrap(list.cellForItem(at: 0))
    XCTAssertEqual(replacement.accessibilityCustomActions?.map(\.name), ["Archive replacement"])
  }

  func testProviderDetachCannotOpenOrInvokeAStaleSwipeSession() throws {
    let rows = Rows()
    let list = make(rows)
    let window = host(list, size: CGSize(width: 300, height: 200))
    defer { window.isHidden = true }
    let cell = try XCTUnwrap(list.cellForItem(at: 0))
    rows.leadingHook = { $0.removeFromSuperview() }
    list.shadowListKit_swipeItem(at: 0, distance: 300, velocity: 1000)
    XCTAssertNil(list.window)
    XCTAssertEqual(cell.transform, .identity)
    XCTAssertEqual(rows.trailingCalls, 0)
    XCTAssertTrue(rows.actedKeys.isEmpty)
  }
}
