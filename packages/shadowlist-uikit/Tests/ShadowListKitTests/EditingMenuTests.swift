import ShadowListKit
import UIKit
import XCTest

@MainActor
final class EditingMenuTests: XCTestCase {
  private final class Rows: NSObject, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
    var offerMenu = true
    var menus = 0
    var selections = 0
    var moves = 0
    func numberOfItems(in listView: ShadowListKitListView) -> Int {
      10
    }

    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
      "item-\(index)"
    }

    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "row")
    }

    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
      40
    }

    func listView(_ listView: ShadowListKitListView, contextMenuForItemAt index: Int) -> UIMenu? {
      menus += 1
      XCTAssertEqual(index, 0)
      return offerMenu ? UIMenu(children: [UIAction(title: "Inspect") { _ in }]) : nil
    }

    func listView(_ listView: ShadowListKitListView, didSelectItemAt index: Int) {
      selections += 1
    }

    func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
      moves += 1
    }
  }

  private func menu(editing: Bool, offered: Bool) throws {
    let rows = Rows()
    rows.offerMenu = offered
    let list = ShadowListKitListView()
    list.contentInsetAdjustmentBehavior = .never
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.dataSource = rows
    list.delegate = rows
    list.reorderEnabled = editing
    list.isEditing = editing
    list.reloadData()
    let window = host(list, size: CGSize(width: 300, height: 300))
    defer { window.isHidden = true }
    if editing {
      list.shadowListKit_beginDrag(at: CGPoint(x: 100, y: 20))
      list.shadowListKit_endDrag()
      XCTAssertEqual(rows.menus, 0)
      XCTAssertEqual(rows.moves, 0)
    }
    let interaction = try XCTUnwrap(list.interactions.compactMap { $0 as? UIContextMenuInteraction }.first)
    // Invoke UIKit's installed menu delegate, rather than calling the app's provider directly.
    let configuration = interaction.delegate?.contextMenuInteraction(
      interaction,
      configurationForMenuAtLocation: CGPoint(
        x: 100,
        y: 20
      )
    )
    XCTAssertEqual(configuration != nil, offered)
    XCTAssertEqual(rows.menus, 1)
    XCTAssertEqual(rows.selections, 0)
    XCTAssertTrue(list.selectedIndices.isEmpty)
    withExtendedLifetime(rows) {}
  }

  func testSuppliedMenuRemainsAvailable() throws {
    try menu(editing: false, offered: true)
  }

  func testEditingDisablesReorderAndPreservesSuppliedMenu() throws {
    try menu(editing: true, offered: true)
  }

  func testDeclinedMenuStaysAbsentWhileEditing() throws {
    try menu(editing: true, offered: false)
  }
}
