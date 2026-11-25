import AppKit
import ShadowListKitAppKit
import XCTest

@MainActor
final class StickyHeaderOverlayTests: XCTestCase {
  private final class Header: NSView {
    override var fittingSize: NSSize {
      NSSize(width: 0, height: 60)
    }
  }

  private final class Action: NSObject {
    var presses = 0
    @objc func press(_ sender: NSButton) {
      presses += 1
    }
  }

  func testOffscreenCustomHeaderKeepsIdentityAndRestoresTheOriginalOnDetach() {
    let harness = ListHarness(source: SizedTestSource(count: 20), size: NSSize(width: 200, height: 100)) { list in
      list.stickyIndices = IndexSet([0, 3])
    }
    let list = harness.list
    var made: [Header] = []
    let original = list.cellForItem(at: 0)!
    list.renderStickyHeaderOverlay = { key, index, section in
      XCTAssertEqual(key, "item-0")
      XCTAssertEqual(index, 0)
      XCTAssertEqual(section, NSNotFound)
      let view = Header()
      made.append(view)
      return view
    }
    harness.layout()
    let header = made[0]
    XCTAssertTrue(original.isHidden)
    list.contentOffset.y = 20
    harness.layout()
    harness.resize(width: 300)
    XCTAssertEqual(made.count, 1)
    XCTAssertEqual(header.frame.width, 300)
    list.contentOffset.y = 100
    harness.layout()
    let screen = list.convert(header.bounds, from: header)
    XCTAssertEqual(screen.minY + list.contentOffset.y, 72, accuracy: 0.01)
    list.renderStickyHeaderOverlay = nil
    harness.layout()
    XCTAssertNil(header.superview)
    XCTAssertFalse(original.isHidden)
    list.renderStickyHeaderOverlay = { _, _, _ in Header() }
    harness.layout()
    list.removeFromSuperview()
    XCTAssertFalse(original.isHidden)
  }

  func testDrivenLayerCatchUpKeepsTheOriginalHiddenUntilOverlayRemoval() {
    let harness = ListHarness(source: SizedTestSource(count: 20), size: NSSize(width: 200, height: 100)) { list in
      list.isScrollDriven = true
      list.stickyIndices = IndexSet([0, 3])
    }
    let list = harness.list
    // Query before moving: cellForItem itself catches up the views on AppKit.
    let original = list.cellForItem(at: 0)!
    // The host drags: the list places the cells by their layers until the phase goes idle.
    list.scrollPhase = .dragging
    list.contentOffset.y = 20
    harness.layout()
    XCTAssertEqual(original.value(forKey: "placedByLayer") as? Bool, true)
    list.renderStickyHeaderOverlay = { _, _, _ in Header() }
    harness.layout()
    XCTAssertTrue(original.isHidden)
    XCTAssertEqual(original.layer?.isHidden, true)
    XCTAssertEqual(original.value(forKey: "shown") as? Bool, false)
    list.scrollPhase = .idle
    XCTAssertTrue(original.isHidden)
    XCTAssertEqual(original.layer?.isHidden, true)
    XCTAssertEqual(original.value(forKey: "shown") as? Bool, false)
    XCTAssertNil(list.cellForItem(at: 0))
    list.renderStickyHeaderOverlay = nil
    harness.layout()
    harness.rest()
    XCTAssertFalse(original.isHidden)
    XCTAssertEqual(original.layer?.isHidden, false)
    XCTAssertEqual(original.value(forKey: "shown") as? Bool, true)
    XCTAssertTrue(list.cellForItem(at: 0) === original)
  }

  func testAccessibilityRowUsesTheCustomButtonOnceAndRestoresTheCell() throws {
    let harness = ListHarness(source: SizedTestSource(count: 20), size: NSSize(width: 200, height: 100)) { list in
      list.stickyIndices = IndexSet([0, 3])
    }
    let list = harness.list
    let original = try XCTUnwrap(list.cellForItem(at: 0))
    let action = Action()
    let button = NSButton(title: "Custom section action", target: action, action: #selector(Action.press(_:)))
    list.renderStickyHeaderOverlay = { _, _, _ in button }
    harness.layout()
    let row = try XCTUnwrap(list.accessibilityRows()?.first as? NSAccessibilityElement)
    let nativeChildren = NSAccessibility.unignoredChildren(from: [button])
    XCTAssertTrue(row.accessibilityChildren()?.first as AnyObject? === nativeChildren.first as AnyObject?)
    XCTAssertEqual(row.accessibilityChildren()?.count, 1)
    XCTAssertTrue(button.accessibilityParent() as? NSAccessibilityElement === row)
    button.performClick(nil)
    XCTAssertEqual(action.presses, 1)
    list.contentOffset.y = 100
    harness.layout()
    let viewFrame = try XCTUnwrap(list.window).convertToScreen(button.convert(button.bounds, to: nil))
    XCTAssertEqual(row.accessibilityFrame(), viewFrame)
    XCTAssertTrue(list.accessibilityVisibleRows()?.contains { $0 as? NSAccessibilityElement === row } == true)
    list.setAccessibilityElement(false)
    XCTAssertEqual(list.accessibilityVisibleRows()?.count, 0)
    XCTAssertEqual(list.accessibilityChildren()?.count, 0)
    list.setAccessibilityElement(true)
    list.renderStickyHeaderOverlay = nil
    harness.layout()
    XCTAssertTrue(row.accessibilityChildren()?.first as? ShadowListKitListCell === original)
    XCTAssertNil(button.superview)
    XCTAssertNil(button.accessibilityParent())
  }

  func testDrivenReuseRestoresTheAuthoritativeCellVisibility() {
    let source = SizedTestSource(count: 1)
    let harness = ListHarness(source: source, size: NSSize(width: 200, height: 20)) { list in
      list.isScrollDriven = true
      list.stickyIndices = IndexSet(integer: 0)
    }
    let list = harness.list
    let original = list.cellForItem(at: 0)!
    list.contentOffset.y = 10
    harness.layout()
    var headers: [Header] = []
    list.renderStickyHeaderOverlay = { _, _, _ in let view = Header(); headers.append(view); return view }
    harness.layout()
    source.keys = ["replacement"]
    list.reloadData()
    harness.layout()
    XCTAssertNil(headers[0].superview)
    // A replacement can mount before its predecessor reaches the pool. The next replacement
    // reuses that pooled cell; check its public index before asserting its layer/view state.
    source.keys = ["reused"]
    list.reloadData()
    harness.layout()
    XCTAssertEqual(original.index, 0)
    XCTAssertTrue(original.isHidden)
    XCTAssertEqual(original.layer?.isHidden, true)
    XCTAssertEqual(original.value(forKey: "shown") as? Bool, false)
    list.renderStickyHeaderOverlay = nil
    harness.layout()
    harness.rest()
    XCTAssertFalse(original.isHidden)
    XCTAssertEqual(original.layer?.isHidden, false)
    XCTAssertTrue(list.cellForItem(at: 0) === original)
  }
}
