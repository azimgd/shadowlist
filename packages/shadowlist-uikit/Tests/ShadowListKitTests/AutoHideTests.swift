import ShadowListKit
import UIKit
import XCTest

@MainActor
final class AutoHideTests: XCTestCase {
  /*
   * Exercise the list's existing scroll classification without synthesizing a UIKit pan.
   * Each changed content offset is read in layout while this reports a dragging phase.
   */
  private final class List: ShadowListKitListView {
    var draggingPhase = false
    override var isDragging: Bool {
      draggingPhase
    }
  }

  private final class Source: NSObject, ShadowListKitListViewDataSource {
    var keys = (0 ..< 100).map { "row-\($0)" }
    func numberOfItems(in listView: ShadowListKitListView) -> Int {
      keys.count
    }

    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
      keys[index]
    }

    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "cell")
    }

    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
      40
    }
  }

  private func makeList() -> (List, Source) {
    let list = List(frame: CGRect(x: 0, y: 0, width: 320, height: 400))
    let source = Source()
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "cell")
    list.dataSource = source
    list.headerView = UIView(frame: CGRect(x: 0, y: 0, width: 320, height: 60))
    list.footerView = UIView(frame: CGRect(x: 0, y: 0, width: 320, height: 40))
    list.autoHideHeader = true
    list.autoHideFooter = true
    list.reloadData()
    list.layoutIfNeeded()
    move(list, to: 200, byUser: false)
    return (list, source)
  }

  private func move(_ list: List, to offset: CGFloat, byUser: Bool) {
    list.draggingPhase = byUser
    list.contentOffset = CGPoint(x: 0, y: offset)
    list.setNeedsLayout()
    list.layoutIfNeeded()
    list.draggingPhase = false
  }

  private func headerStart(_ list: List) -> CGFloat {
    list.headerView!.frame.minY - list.contentOffset.y
  }

  private func footerEnd(_ list: List) -> CGFloat {
    list.footerView!.frame.maxY - list.contentOffset.y
  }

  func testOnlyUserDisplacementAccumulatesAndRepeatedLayoutsDoNotHideAgain() {
    let (list, source) = makeList()
    withExtendedLifetime(source) {
      XCTAssertEqual(headerStart(list), 0, accuracy: 0.01)
      move(list, to: 225, byUser: true)
      XCTAssertEqual(headerStart(list), -25, accuracy: 0.01)
      XCTAssertEqual(footerEnd(list), 425, accuracy: 0.01)
      list.setNeedsLayout()
      list.layoutIfNeeded()
      XCTAssertEqual(headerStart(list), -25, accuracy: 0.01)
      move(list, to: 325, byUser: false)
      XCTAssertEqual(headerStart(list), -25, accuracy: 0.01)
      move(list, to: 315, byUser: true)
      XCTAssertEqual(headerStart(list), -15, accuracy: 0.01)
      XCTAssertEqual(footerEnd(list), 415, accuracy: 0.01)
      XCTAssertEqual(list.headerView!.layer.zPosition, 1)
      XCTAssertEqual(list.footerView!.layer.zPosition, 1)
    }
  }

  func testPrependCorrectionDoesNotCountAsFingerMovement() {
    let (list, source) = makeList()
    move(list, to: 225, byUser: true)
    source.keys.insert("new", at: 0)
    list.insertItems(at: IndexSet(integer: 0))
    list.layoutIfNeeded()
    XCTAssertGreaterThan(list.contentOffset.y, 225)
    XCTAssertEqual(headerStart(list), -25, accuracy: 0.01)
    XCTAssertEqual(footerEnd(list), 425, accuracy: 0.01)
  }

  func testFlagChangesRetainIntentAndBoundaryCommandsRevealTheBars() {
    let (list, source) = makeList()
    withExtendedLifetime(source) {
      move(list, to: 225, byUser: true)
      list.autoHideHeader = false
      list.layoutIfNeeded()
      list.autoHideHeader = true
      list.layoutIfNeeded()
      XCTAssertEqual(headerStart(list), -25, accuracy: 0.01)
      list.scrollToStart(animated: false)
      list.layoutIfNeeded()
      XCTAssertEqual(headerStart(list), 0, accuracy: 0.01)
      list.scrollToEnd(animated: false)
      list.layoutIfNeeded()
      XCTAssertEqual(footerEnd(list), 400, accuracy: 0.01)
    }
  }
}
