import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * The overlay scrollers: where the thumbs go, how the pointer grabs them and when they show.
 */
@MainActor
final class ScrollerTests: XCTestCase {
  private final class ScrollerDelegate: NSObject, ShadowListKitScrollerViewDelegate {
    var offsets: [CGPoint] = []

    func scrollerView(_ scrollerView: ShadowListKitScrollerView, didScrollTo offset: CGPoint) {
      offsets.append(offset)
      scrollerView.offset = offset
    }
  }

  private var window: NSWindow!
  private let delegate = ScrollerDelegate()

  /*
   * A 300 by 500 scroller view over 1200 by 2000 of content, in an offscreen window.
   */
  private func makeScrollers(axes: ShadowListKitScrollAxes = .vertical) -> ShadowListKitScrollerView {
    window = NSWindow(
      contentRect: NSRect(x: -10_000, y: -10_000, width: 300, height: 500), styleMask: [.borderless], backing: .buffered,
      defer: false)
    window.isReleasedWhenClosed = false
    let scrollers = ShadowListKitScrollerView(frame: NSRect(x: 0, y: 0, width: 300, height: 500))
    scrollers.shadowListKit_setScrollerStyle(.overlay)
    scrollers.axes = axes
    scrollers.delegate = delegate
    scrollers.contentSize = CGSize(width: 1200, height: 2000)
    window.contentView!.addSubview(scrollers)
    scrollers.flashScrollers()
    return scrollers
  }

  private func mouse(_ type: NSEvent.EventType, at point: NSPoint, in view: NSView) -> NSEvent {
    let location = view.convert(point, to: nil)
    return NSEvent.mouseEvent(
      with: type, location: location, modifierFlags: [], timestamp: 0, windowNumber: view.window!.windowNumber,
      context: nil, eventNumber: 0, clickCount: 1, pressure: 1)!
  }

  func testThumbsFollowTheOffset() {
    let scrollers = makeScrollers(axes: [.horizontal, .vertical])
    // 500 of 2000 shown: a quarter of the 496 point track, 2 in from the edges.
    XCTAssertEqual(scrollers.shadowListKit_verticalThumbFrame, CGRect(x: 291, y: 2, width: 7, height: 124))
    XCTAssertEqual(scrollers.shadowListKit_horizontalThumbFrame, CGRect(x: 2, y: 491, width: 74, height: 7))
    scrollers.offset = CGPoint(x: 900, y: 1500)
    XCTAssertEqual(scrollers.shadowListKit_verticalThumbFrame.maxY, 498, accuracy: 0.01)
    XCTAssertEqual(scrollers.shadowListKit_horizontalThumbFrame.maxX, 298, accuracy: 0.01)
    // Past the end the thumb shortens.
    scrollers.offset = CGPoint(x: 900, y: 1550)
    XCTAssertEqual(scrollers.shadowListKit_verticalThumbFrame.height, 74, accuracy: 0.01)
    // Insets keep the track off the edges.
    scrollers.scrollerInsets = NSEdgeInsets(top: 40, left: 0, bottom: 0, right: 0)
    scrollers.offset = .zero
    XCTAssertEqual(scrollers.shadowListKit_verticalThumbFrame.minY, 42, accuracy: 0.01)
    // Content that fits has no thumb.
    scrollers.contentSize = CGSize(width: 300, height: 300)
    XCTAssertTrue(scrollers.shadowListKit_verticalThumbFrame.isNull)
    XCTAssertTrue(scrollers.shadowListKit_horizontalThumbFrame.isNull)
  }

  func testThumbUnderThePointerGrowsWider() {
    let scrollers = makeScrollers()
    scrollers.mouseMoved(with: mouse(.mouseMoved, at: NSPoint(x: 296, y: 300), in: scrollers))
    XCTAssertEqual(scrollers.shadowListKit_verticalThumbFrame, CGRect(x: 287, y: 2, width: 11, height: 124))
    // Hovered, it stays shown.
    scrollers.shadowListKit_hideScrollers()
    XCTAssertFalse(scrollers.shadowListKit_verticalThumbFrame.isNull)
    scrollers.mouseMoved(with: mouse(.mouseMoved, at: NSPoint(x: 100, y: 300), in: scrollers))
    XCTAssertEqual(scrollers.shadowListKit_verticalThumbFrame.width, 7)
  }

  func testClickOnTheTrackMovesTheThumbThereAndDragsIt() {
    let scrollers = makeScrollers()
    scrollers.mouseDown(with: mouse(.leftMouseDown, at: NSPoint(x: 295, y: 400), in: scrollers))
    // The thumb's middle goes to the pointer: its start at 338 of a 372 point free track.
    let offset = try? XCTUnwrap(delegate.offsets.last)
    XCTAssertEqual(offset?.y ?? 0, (400 - 62 - 2) / 372 * 1500, accuracy: 0.01)
    XCTAssertEqual(scrollers.shadowListKit_verticalThumbFrame.midY, 400, accuracy: 0.5)
    scrollers.mouseDragged(with: mouse(.leftMouseDragged, at: NSPoint(x: 280, y: 1000), in: scrollers))
    XCTAssertEqual(delegate.offsets.last?.y ?? 0, 1500, accuracy: 0.01)
    scrollers.mouseUp(with: mouse(.leftMouseUp, at: NSPoint(x: 280, y: 1000), in: scrollers))
    // A press on the thumb holds it where it pressed.
    delegate.offsets.removeAll()
    scrollers.mouseDown(with: mouse(.leftMouseDown, at: NSPoint(x: 295, y: 420), in: scrollers))
    XCTAssertTrue(delegate.offsets.isEmpty)
    scrollers.mouseDragged(with: mouse(.leftMouseDragged, at: NSPoint(x: 295, y: 400), in: scrollers))
    XCTAssertEqual(delegate.offsets.last?.y ?? 0, 1500 - 20.0 / 372 * 1500, accuracy: 0.01)
  }

  func testOnlyShownTracksTakeTheMouse() {
    let scrollers = makeScrollers()
    let superview = scrollers.superview!
    XCTAssertTrue(scrollers.hitTest(scrollers.convert(NSPoint(x: 295, y: 250), to: superview)) === scrollers)
    XCTAssertNil(scrollers.hitTest(scrollers.convert(NSPoint(x: 150, y: 250), to: superview)))
    scrollers.shadowListKit_hideScrollers()
    XCTAssertNil(scrollers.hitTest(scrollers.convert(NSPoint(x: 295, y: 250), to: superview)))
  }

  func testOverlayThumbsFadeAndAlwaysThumbsStay() {
    let scrollers = makeScrollers()
    XCTAssertFalse(scrollers.showsScrollersAlways)
    scrollers.shadowListKit_hideScrollers()
    XCTAssertTrue(scrollers.shadowListKit_verticalThumbFrame.isNull)
    scrollers.offset = CGPoint(x: 0, y: 10)
    XCTAssertFalse(scrollers.shadowListKit_verticalThumbFrame.isNull)

    scrollers.shadowListKit_setScrollerStyle(.legacy)
    XCTAssertTrue(scrollers.showsScrollersAlways)
    scrollers.shadowListKit_hideScrollers()
    XCTAssertFalse(scrollers.shadowListKit_verticalThumbFrame.isNull)
  }
}
