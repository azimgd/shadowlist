import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * The header and footer pinned to the viewport, the empty view of a list without items, the
 * items that never hold the content and whether an inverted list is at its end.
 */
@MainActor
final class ListTemplatesTests: XCTestCase {
  /*
   * Records what the list says about its end.
   */
  private final class EndRecorder: NSObject, ShadowListKitListViewDelegate {
    var changes: [Bool] = []

    func listView(_ listView: ShadowListKitListView, didChangeAtEnd atEnd: Bool) {
      changes.append(atEnd)
    }
  }

  /*
   * A view's frame in the list's coordinates.
   */
  private func frame(_ view: NSView, in list: ShadowListKitListView) -> CGRect {
    list.convert(view.frame, from: view.superview)
  }

  private func scroll(_ harness: ListHarness, to y: CGFloat) {
    harness.list.contentOffset = CGPoint(x: 0, y: y)
    harness.layout()
  }

  /*
   * A trackpad gesture that moves the list by delta, the reader's own scrolling.
   */
  private func drag(_ harness: ListHarness, by delta: CGFloat) {
    let list = harness.list
    list.shadowListKit_scrollWheelDelta(.zero, phase: .began, momentumPhase: [])
    list.shadowListKit_scrollWheelDelta(CGPoint(x: 0, y: -delta), phase: .changed, momentumPhase: [])
    harness.layout()
    list.shadowListKit_scrollWheelDelta(.zero, phase: .ended, momentumPhase: [])
    harness.layout()
  }

  func testAutoHideBarsAccumulateOnlyTrackpadMovementAndRetainIntentAcrossCommands() throws {
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 60))
    let footer = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 40))
    let harness = ListHarness(source: SizedTestSource(count: 100)) {
      $0.headerView = header
      $0.footerView = footer
      $0.autoHideHeader = true
      $0.autoHideFooter = true
    }
    scroll(harness, to: 200)
    XCTAssertEqual(frame(header, in: harness.list).minY, 0, accuracy: 0.01)
    drag(harness, by: 25)
    XCTAssertEqual(frame(header, in: harness.list).minY, -25, accuracy: 0.01)
    XCTAssertEqual(frame(footer, in: harness.list).maxY, 505, accuracy: 0.01)
    harness.list.setContentOffset(CGPoint(x: 0, y: 325), animated: false)
    harness.layout()
    XCTAssertEqual(frame(header, in: harness.list).minY, -25, accuracy: 0.01)
    harness.list.autoHideHeader = false
    harness.layout()
    harness.list.autoHideHeader = true
    harness.layout()
    XCTAssertEqual(frame(header, in: harness.list).minY, -25, accuracy: 0.01)
    harness.list.scrollToStart(animated: false)
    harness.layout()
    XCTAssertEqual(frame(header, in: harness.list).minY, 0, accuracy: 0.01)
  }

  func testAStickyHeaderStaysAtTheViewportStartUntilTheFooterPushesItOff() throws {
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 60))
    let footer = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 50))
    let harness = ListHarness(source: SizedTestSource(count: 100)) {
      $0.headerView = header
      $0.footerView = footer
      $0.stickyHeader = true
    }
    let list = harness.list
    XCTAssertEqual(frame(header, in: list), CGRect(x: 0, y: 0, width: 320, height: 60))
    for offset: CGFloat in [40, 300, 2000] {
      scroll(harness, to: offset)
      XCTAssertEqual(frame(header, in: list).minY, 0, accuracy: 0.01, "offset \(offset)")
    }
    // It sits over the items it covers.
    XCTAssertEqual(header.layer?.zPosition, 1)
    XCTAssertEqual(list.rectForItem(at: 0).minY, 60, accuracy: 0.01)

    list.stickyHeader = false
    harness.layout()
    XCTAssertEqual(frame(header, in: list).minY, -list.contentOffset.y, accuracy: 0.01)
    XCTAssertEqual(header.layer?.zPosition, 0)
    list.stickyHeader = true

    // In a viewport shorter than the two, the footer pushes the header off at the end.
    harness.window.setFrame(NSRect(x: -10_000, y: -10_000, width: 320, height: 100), display: false)
    harness.layout()
    list.scrollToEnd(animated: false)
    harness.layout()
    XCTAssertEqual(frame(footer, in: list).maxY, 100, accuracy: 0.01)
    XCTAssertEqual(frame(header, in: list).maxY, frame(footer, in: list).minY, accuracy: 0.01)
  }

  func testAStickyFooterStaysAtTheViewportEnd() throws {
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 60))
    let footer = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 50))
    let harness = ListHarness(source: SizedTestSource(count: 100)) {
      $0.headerView = header
      $0.footerView = footer
      $0.stickyFooter = true
    }
    let list = harness.list
    for offset: CGFloat in [0, 300, 2000] {
      scroll(harness, to: offset)
      XCTAssertEqual(frame(footer, in: list).maxY, 480, accuracy: 0.01, "offset \(offset)")
    }
    list.scrollToEnd(animated: false)
    harness.layout()
    XCTAssertEqual(frame(footer, in: list).minY, frame(try XCTUnwrap(list.cellForItem(at: 99)), in: list).maxY, accuracy: 0.01)
  }

  func testAStickyFooterOfAShortListRestsAtTheViewportEndAndUnderTheHeader() throws {
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 60))
    let footer = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 50))
    let harness = ListHarness(source: SizedTestSource(count: 2)) {
      $0.headerView = header
      $0.footerView = footer
      $0.stickyFooter = true
    }
    let list = harness.list
    XCTAssertEqual(frame(footer, in: list).maxY, 480, accuracy: 0.01)
    XCTAssertEqual(list.contentSize.height, 60 + 2 * 44 + 50, accuracy: 0.01)

    // A viewport shorter than the header and the footer keeps the footer under the header.
    harness.window.setFrame(NSRect(x: -10_000, y: -10_000, width: 320, height: 80), display: false)
    harness.layout()
    XCTAssertEqual(frame(footer, in: list).minY, 60, accuracy: 0.01)
  }

  func testTheEmptyViewShowsAfterTheHeaderWhileTheListHasNoItems() throws {
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 60))
    let footer = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 50))
    let empty = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 200))
    empty.setAccessibilityElement(true)
    empty.setAccessibilityRole(.group)
    let source = SizedTestSource(count: 0)
    let harness = ListHarness(source: source) {
      $0.headerView = header
      $0.footerView = footer
      $0.emptyView = empty
    }
    let list = harness.list
    XCTAssertFalse(empty.isHidden)
    XCTAssertEqual(frame(empty, in: list), CGRect(x: 0, y: 60, width: 320, height: 200))
    XCTAssertEqual(frame(footer, in: list).minY, 260, accuracy: 0.01)
    XCTAssertEqual(list.contentSize.height, 310, accuracy: 0.01)
    XCTAssertTrue((list.accessibilityChildren() ?? []).contains { ($0 as? NSView) === empty })

    source.keys = ["item-0", "item-1"]
    list.insertItems(at: IndexSet(integersIn: 0...1))
    harness.layout()
    XCTAssertTrue(empty.isHidden)
    XCTAssertFalse((list.accessibilityChildren() ?? []).contains { ($0 as? NSView) === empty })
    XCTAssertEqual(list.rectForItem(at: 0).minY, 60, accuracy: 0.01)
    XCTAssertEqual(frame(footer, in: list).minY, 60 + 2 * 44, accuracy: 0.01)
    XCTAssertEqual(list.contentSize.height, 60 + 2 * 44 + 50, accuracy: 0.01)

    source.keys = []
    list.reloadData()
    harness.layout()
    XCTAssertFalse(empty.isHidden)
    XCTAssertEqual(frame(footer, in: list).minY, 260, accuracy: 0.01)

    list.emptyView = nil
    harness.layout()
    XCTAssertNil(empty.superview)
    XCTAssertEqual(frame(footer, in: list).minY, 60, accuracy: 0.01)
    XCTAssertEqual(list.contentSize.height, 110, accuracy: 0.01)
  }

  func testAnEmptyViewSetOnAListWithItemsStaysHidden() {
    let empty = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 200))
    let harness = ListHarness(source: SizedTestSource(count: 5))
    harness.list.emptyView = empty
    harness.layout()
    XCTAssertTrue(empty.isHidden)
    XCTAssertEqual(harness.list.contentSize.height, 5 * 44, accuracy: 0.01)
  }

  func testANonAnchorItemNeverHoldsTheVisibleItems() throws {
    for nonAnchor in [false, true] {
      let source = SizedTestSource(count: 100)
      let harness = ListHarness(source: source)
      let list = harness.list
      if nonAnchor {
        list.nonAnchorKeys = ["item-10"]
        XCTAssertEqual(list.nonAnchorKeys, ["item-10"])
      }
      scroll(harness, to: 440)
      XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 10)), 0, accuracy: 0.01)
      // The item at the viewport start grows.
      source.heights["item-10"] = 100
      list.reloadItems(at: IndexSet(integer: 10))
      harness.layout()
      if nonAnchor {
        // The item after it holds the content.
        XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 11)), 44, accuracy: 0.01)
      } else {
        XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 10)), 0, accuracy: 0.01)
      }
    }
  }

  func testAnInvertedListSaysWhenItLeavesItsEndAndIsBack() throws {
    let recorder = EndRecorder()
    let source = SizedTestSource(count: 200)
    let harness = ListHarness(source: source) { $0.isInverted = true }
    let list = harness.list
    list.delegate = recorder
    let end = list.contentSize.height - 480
    XCTAssertEqual(list.contentOffset.y, end, accuracy: 0.5)
    XCTAssertTrue(list.isAtEnd)

    // Items appended while the list rests at its end do not leave it.
    source.keys.append(contentsOf: ["new-0", "new-1"])
    list.insertItems(at: IndexSet(integersIn: 200...201))
    harness.layout()
    XCTAssertTrue(list.isAtEnd)
    XCTAssertEqual(recorder.changes, [])

    drag(harness, by: -600)
    XCTAssertLessThan(list.contentOffset.y, end - 300)
    XCTAssertFalse(list.isAtEnd)
    XCTAssertEqual(recorder.changes, [false])

    list.scrollToEnd(animated: false)
    harness.layout()
    XCTAssertTrue(list.isAtEnd)
    XCTAssertEqual(recorder.changes, [false, true])

    // A scroll command that lands away from the end leaves it.
    list.scrollToItem(at: 20, viewPosition: 0, animated: false)
    harness.layout()
    XCTAssertFalse(list.isAtEnd)
    XCTAssertEqual(recorder.changes, [false, true, false])
  }

  func testAListThatIsNotInvertedIsAlwaysAtItsEnd() {
    let recorder = EndRecorder()
    let harness = ListHarness(source: SizedTestSource(count: 200))
    harness.list.delegate = recorder
    drag(harness, by: 600)
    XCTAssertGreaterThan(harness.list.contentOffset.y, 300)
    XCTAssertTrue(harness.list.isAtEnd)
    XCTAssertEqual(recorder.changes, [])
  }
}
