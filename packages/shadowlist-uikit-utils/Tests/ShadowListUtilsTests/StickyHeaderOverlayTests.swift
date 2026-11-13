import ShadowListKit
import UIKit
import XCTest

@MainActor
final class StickyHeaderOverlayTests: XCTestCase {
  private class Source: NSObject, ShadowListKitListViewDataSource {
    var keys = (0..<20).map { "item-\($0)" }
    var title = "Custom header"
    func numberOfItems(in listView: ShadowListKitListView) -> Int { keys.count }
    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }
    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "row")
    }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 40 }
  }
  private final class EmptySection: Source {
    override init() { super.init(); keys = [] }
    func numberOfSections(in listView: ShadowListKitListView) -> Int { 1 }
    func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { 0 }
    func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { "Empty section" }
    func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat { 40 }
  }
  private final class Button: UIButton {
    override func sizeThatFits(_ size: CGSize) -> CGSize {
      size.width == CGFloat.greatestFiniteMagnitude ? CGSize(width: 60, height: size.height) : CGSize(width: size.width, height: 60)
    }
  }
  private func fixture(_ source: Source) -> (ShadowListKitListView, UIWindow) {
    let list = ShadowListKitListView()
    list.contentInsetAdjustmentBehavior = .never
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.dataSource = source
    list.stickyIndices = IndexSet([0, 3])
    list.reloadData()
    return (list, host(list, size: CGSize(width: 200, height: 100)))
  }
  private func layout(_ list: ShadowListKitListView) {
    list.setNeedsLayout()
    list.layoutIfNeeded()
  }

  func testCustomViewKeepsIdentityAcrossScrollAndWidthAndUsesItsOwnPushOffExtent() {
    let source = Source()
    let (list, window) = fixture(source)
    var made: [Button] = []
    let original = list.cellForItem(at: 0)!
    list.renderStickyHeaderOverlay = { key, index, section in
      XCTAssertEqual(key, "item-0")
      XCTAssertEqual(index, 0)
      XCTAssertEqual(section, NSNotFound)
      let button = Button()
      made.append(button)
      return button
    }
    layout(list)
    let button = made[0]
    XCTAssertTrue(original.isHidden)
    list.contentOffset.y = 20
    layout(list)
    XCTAssertEqual(button.frame.minY, 20, accuracy: 0.01)
    list.frame.size.width = 300
    layout(list)
    XCTAssertEqual(made.count, 1)
    XCTAssertTrue(made[0] === button)
    XCTAssertEqual(button.frame.width, 300)
    list.contentOffset.y = 100
    layout(list)
    XCTAssertEqual(button.frame.minY, 60, accuracy: 0.01) // Next header120 minus custom extent60.
    list.renderStickyHeaderOverlay = nil
    layout(list)
    XCTAssertNil(button.superview)
    XCTAssertFalse(original.isHidden)
    withExtendedLifetime((window, source)) {}
  }

  func testPayloadRefreshAndActiveReplacementRemoveTheOldView() {
    let source = Source()
    let (list, window) = fixture(source)
    var made: [Button] = []
    list.renderStickyHeaderOverlay = { key, index, _ in
      XCTAssertEqual(key, source.keys[index])
      let button = Button()
      button.setTitle(source.title + ":" + (key ?? ""), for: .normal)
      made.append(button)
      return button
    }
    layout(list)
    let first = made[0]
    source.title = "Updated"
    list.reloadItems(at: IndexSet(integer: 0), payload: "title")
    layout(list)
    XCTAssertNil(first.superview)
    XCTAssertEqual(made.last?.title(for: .normal), "Updated:item-0")
    let payloadView = made.last!
    source.keys.swapAt(0, 1)
    list.reloadData()
    layout(list)
    XCTAssertNil(payloadView.superview)
    XCTAssertEqual(made.last?.title(for: .normal), "Updated:item-1")
    let updated = made.last!
    list.contentOffset.y = 120
    layout(list)
    XCTAssertNil(updated.superview)
    XCTAssertEqual(made.last?.title(for: .normal), "Updated:item-3")
    let active = made.last!
    list.removeFromSuperview()
    XCTAssertNil(active.superview)
    withExtendedLifetime((window, source)) {}
  }

  func testInteractiveOverlayHasOneRepresentationAndNilFactoryRestoresTheOriginal() {
    let source = Source()
    let (list, window) = fixture(source)
    let button = Button()
    button.accessibilityLabel = "Custom section action"
    button.isAccessibilityElement = true
    var presses = 0
    button.addAction(UIAction { _ in presses += 1 }, for: .touchUpInside)
    let original = list.cellForItem(at: 0)!
    list.renderStickyHeaderOverlay = { _, _, _ in button }
    layout(list)
    XCTAssertTrue(original.isHidden)
    XCTAssertTrue(button.isAccessibilityElement)
    XCTAssertTrue(list.accessibilityElement(at: 0) as? UIView === button)
    XCTAssertEqual(list.index(ofAccessibilityElement: button), 0)
    let point = button.convert(CGPoint(x: 20, y: 20), to: window)
    let hit = window.hitTest(point, with: nil)
    XCTAssertTrue(hit === button || hit?.isDescendant(of: button) == true)
    button.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 1)
    list.renderStickyHeaderOverlay = { _, _, _ in nil }
    layout(list)
    XCTAssertFalse(original.isHidden)
    XCTAssertNil(button.superview)
    XCTAssertTrue(list.accessibilityElement(at: 0) as? UIView === original)
    withExtendedLifetime((window, source)) {}
  }

  func testAnonymousEmptySectionExposesNilKeyAndThePublicSectionIndex() {
    let source = EmptySection()
    let (list, window) = fixture(source)
    list.stickyIndices = []
    list.stickySectionHeaders = true
    var calls = 0
    list.renderStickyHeaderOverlay = { key, index, section in
      calls += 1
      XCTAssertNil(key)
      XCTAssertEqual(index, NSNotFound)
      XCTAssertEqual(section, 0)
      return Button()
    }
    layout(list)
    XCTAssertEqual(calls, 1)
    withExtendedLifetime((window, source)) {}
  }
  func testHorizontalOverlayUsesTheExistingLeadingAxisAndCrossSize() {
    let source = Source()
    let (list, window) = fixture(source)
    list.isHorizontal = true
    layout(list)
    let button = Button()
    list.renderStickyHeaderOverlay = { _, _, _ in button }
    layout(list)
    XCTAssertEqual(button.frame.width, 60)
    XCTAssertEqual(button.frame.height, 100)
    list.contentOffset.x = 100
    layout(list)
    XCTAssertEqual(button.frame.minX, 60, accuracy: 0.01)
    withExtendedLifetime((window, source)) {}
  }

  func testFactoryDetachmentDoesNotAttachItsReturnedViewAfterCleanup() {
    let source = Source()
    let (list, window) = fixture(source)
    let button = Button()
    var calls = 0
    list.renderStickyHeaderOverlay = { _, _, _ in
      calls += 1
      list.removeFromSuperview()
      return button
    }
    layout(list)
    XCTAssertEqual(calls, 1)
    XCTAssertNil(button.superview)
    XCTAssertNil(list.window)
    withExtendedLifetime((window, source)) {}
  }

  func testFactoryCanClearItselfWithoutLandingAnObsoleteView() {
    let source = Source()
    let (list, window) = fixture(source)
    let original = list.cellForItem(at: 0)!
    let button = Button()
    list.renderStickyHeaderOverlay = { _, _, _ in
      list.renderStickyHeaderOverlay = nil
      return button
    }
    layout(list)
    XCTAssertNil(button.superview)
    XCTAssertFalse(original.isHidden)
    withExtendedLifetime((window, source)) {}
  }

}
