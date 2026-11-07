import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * The window around the list: backing scale, appearance and resizing.
 */
@MainActor
final class EnvironmentTests: XCTestCase {
  /*
   * Rows as tall as wrapped text: the narrower the list, the taller each row.
   */
  private final class WrappingSource: TestSource {
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
      let lines = ceil(CGFloat(40 + (index * 13) % 160) * 8 / max(1, crossSize))
      return 12 + lines * 16
    }
  }

  private func separator(of cell: ShadowListKitListCell) -> CALayer? {
    cell.layer?.sublayers?.first { $0.zPosition == 1000 && !$0.isHidden }
  }

  private func separatorHarness() -> ListHarness {
    ListHarness(source: SizedTestSource(count: 100)) { $0.showsSeparators = true }
  }

  func testSeparatorIsOneDevicePixelAndFollowsTheBackingScale() throws {
    let harness = separatorHarness()
    let list = harness.list
    let cell = try XCTUnwrap(list.cellForItem(at: 2))
    let line = try XCTUnwrap(separator(of: cell))
    XCTAssertEqual(line.frame.height, 0.5)
    XCTAssertEqual(line.frame.minX, 16)
    XCTAssertEqual(line.frame.maxY, cell.bounds.height, accuracy: 0.001)

    harness.window.scale = 1
    list.viewDidChangeBackingProperties()
    harness.layout()
    XCTAssertEqual(try XCTUnwrap(separator(of: cell)).frame.height, 1)
    XCTAssertEqual(try XCTUnwrap(separator(of: cell)).frame.maxY, cell.bounds.height, accuracy: 0.001)
  }

  func testAppearanceChangeResolvesTheSeparatorColorAgain() throws {
    let harness = separatorHarness()
    let list = harness.list
    func resolved(_ name: NSAppearance.Name) -> CGColor {
      var color: CGColor!
      NSAppearance(named: name)!.performAsCurrentDrawingAppearance { color = NSColor.separatorColor.cgColor }
      return color
    }
    list.appearance = NSAppearance(named: .aqua)
    harness.layout()
    let cell = try XCTUnwrap(list.cellForItem(at: 1))
    XCTAssertEqual(try XCTUnwrap(separator(of: cell)).backgroundColor, resolved(.aqua))
    list.appearance = NSAppearance(named: .darkAqua)
    harness.layout()
    XCTAssertEqual(try XCTUnwrap(separator(of: cell)).backgroundColor, resolved(.darkAqua))
    XCTAssertNotEqual(resolved(.aqua), resolved(.darkAqua))
  }

  func testResizeAcrossTheAxisKeepsTheFirstVisibleRow() throws {
    let harness = ListHarness(source: WrappingSource(count: 600))
    let list = harness.list
    for step in 1...12 {
      list.contentOffset = CGPoint(x: 0, y: CGFloat(step) * 400 + 7)
      harness.layout()
    }
    let anchor = list.visibleRange.location
    let y = try XCTUnwrap(harness.screenY(ofItem: anchor))
    let height = list.rectForItem(at: anchor).height
    for width: CGFloat in [300, 260, 220, 200] {
      harness.resize(width: width)
      XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: anchor)), y, accuracy: 0.5, "width \(width)")
    }
    XCTAssertGreaterThan(list.rectForItem(at: anchor).height, height)
    XCTAssertEqual(try XCTUnwrap(list.cellForItem(at: anchor)).frame.width, 200)
    list.viewDidEndLiveResize()
    harness.layout()
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: anchor)), y, accuracy: 0.5)
  }
}
