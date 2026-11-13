import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class TreeTests: XCTestCase {
  private var items: [ShadowListUtilsTreeNode] {
    [ShadowListUtilsTreeNode(key: "root", name: "Sources", children: [
      ShadowListUtilsTreeNode(key: "branch", name: "Views", children: [ShadowListUtilsTreeNode(key: "🦊", name: "Fox.swift")]),
      ShadowListUtilsTreeNode(key: "leaf", name: "Main.swift"),
    ]), ShadowListUtilsTreeNode(key: "empty", name: "Empty", children: [])]
  }

  func testExpansionAndHiddenBranchesUseSharedRows() {
    let list = ShadowListUtilsTreeList(theme: .light)
    list.items = items
    XCTAssertEqual(list.rows.map(\.key), ["root", "empty"])
    list.toggle("root")
    list.toggle("branch")
    XCTAssertEqual(list.rows.map(\.key), ["root", "branch", "🦊", "leaf", "empty"])
    let row = list.rows[2] as! ShadowListUtilsTreeRow
    XCTAssertEqual(row.depth, 2)
    list.toggle("root")
    XCTAssertEqual(list.rows.map(\.key), ["root", "empty"])
    list.toggle("root")
    XCTAssertEqual(list.rows.map(\.key), ["root", "branch", "🦊", "leaf", "empty"])
  }

  func testControlledExpansionWaitsForTheApplication() {
    let list = ShadowListUtilsTreeList(theme: .light)
    list.items = items
    list.expandedKeys = []
    var requested: Set<String> = []
    list.onExpandedChange = { requested = $0 }
    list.toggle("root")
    XCTAssertEqual(requested, ["root"])
    XCTAssertEqual(list.rows.map(\.key), ["root", "empty"])
    list.expandedKeys = requested
    XCTAssertEqual(list.rows.map(\.key), ["root", "branch", "leaf", "empty"])
    list.onExpandedChange = { list.expandedKeys = $0 }
    list.toggle("branch")
    XCTAssertEqual(list.rows.map(\.key), ["root", "branch", "🦊", "leaf", "empty"])
  }

  func testInitialExpansionLeafPressAndEmptyFolder() {
    let list = ShadowListUtilsTreeList(theme: .light, initialExpandedKeys: ["root", "branch"])
    list.items = items
    XCTAssertEqual(list.rows.map(\.key), ["root", "branch", "🦊", "leaf", "empty"])
    var pressed: [String] = []
    list.onPressItem = { pressed.append($0.key) }
    list.didSelectItem(at: 2)
    XCTAssertEqual(pressed, ["🦊"])
    list.toggle("empty")
    XCTAssertEqual(list.rows.count, 5)
    list.items = items
    XCTAssertEqual(list.rows.count, 5)
  }

  func testCustomRowsReceiveNativeTreeMetadataAndToggle() {
    let list = ShadowListUtilsTreeList(theme: .light)
    list.items = items
    var root: ShadowListUtilsTreeListRenderItemInfo?
    list.rowForItem = { info in
      if info.item.key == "root" { root = info }
      return ShadowListUtilsTreeRow(item: info.item, depth: info.depth, hasChildren: info.hasChildren,
                                   isExpanded: info.isExpanded, theme: .light)
    }
    XCTAssertTrue(root?.hasChildren ?? false)
    root?.toggle()
    XCTAssertEqual(list.rows.count, 4)
    XCTAssertEqual(root?.indent, 0)
  }

  func testControlledRequestsComposeAndUncontrolledStateRemainsSeparate() {
    let list = ShadowListUtilsTreeList(theme: .light, initialExpandedKeys: ["root"])
    list.items = items
    list.expandedKeys = []
    var requests: [Set<String>] = []
    list.onExpandedChange = { requests.append($0) }
    list.toggle("root")
    list.toggle("root")
    XCTAssertEqual(requests, [["root"], []])
    XCTAssertEqual(list.rows.map(\.key), ["root", "empty"])
    list.expandedKeys = nil
    XCTAssertEqual(list.rows.map(\.key), ["root", "branch", "leaf", "empty"])
  }

  func testExpansionIntentSurvivesDataArrivingLater() {
    let list = ShadowListUtilsTreeList(theme: .light, initialExpandedKeys: ["future"])
    list.items = items
    list.items = items + [ShadowListUtilsTreeNode(key: "future", name: "Loaded later", children: [ShadowListUtilsTreeNode(key: "later", name: "Later.txt")])]
    XCTAssertEqual(list.rows.suffix(2).map(\.key), ["future", "later"])
  }

  func testRowAccessibilityAndLargeTextLayout() {
    let row = ShadowListUtilsTreeRow(item: items[0], depth: 1, hasChildren: true, isExpanded: true,
                                    interactive: true, theme: .light, labels: ShadowListUtilsTreeLabels(expand: "Open", collapse: "Close"))
    let layout = row.layout(width: 240)
    XCTAssertGreaterThanOrEqual(layout.height, 44)
    let view = ShadowListUtilsTreeRowView(reuseIdentifier: row.reuseIdentifier)
    view.configure(row, layout: layout)
    XCTAssertEqual(view.accessibilityLabel, "Sources")
    XCTAssertEqual(view.accessibilityHint, "Close")
    XCTAssertTrue(view.accessibilityTraits.contains(.button))
  }
}
