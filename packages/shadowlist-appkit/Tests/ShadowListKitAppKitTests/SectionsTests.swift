import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Sticky section headers and items, the built-in title cells, and the header and footer views.
 */
@MainActor
final class SectionsTests: XCTestCase {
  /*
   * Ten sections of ten 44 point items under a 30 point titled header. A section spans 470
   * points.
   */
  private final class Sections: TestSource {
    var sectionCount = 10
    var perSection = 10
    var footers = false

    func numberOfSections(in listView: ShadowListKitListView) -> Int { sectionCount }
    func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { perSection }
    func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { "Section \(section)" }
    func listView(_ listView: ShadowListKitListView, titleForFooterInSection section: Int) -> String? {
      footers ? "\(perSection) items" : nil
    }
    func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat { 30 }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 44 }
  }

  /*
   * Applies a driven list's offsets and moves items in its source.
   */
  private final class Host: NSObject, ShadowListKitListViewDelegate {
    let source: TestSource
    var corrections = 0

    init(source: TestSource) {
      self.source = source
    }

    func listView(_ listView: ShadowListKitListView, didCorrectOffset contentOffset: CGPoint) {
      corrections += 1
      listView.contentOffset = contentOffset
    }

    func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
      let key = source.keys.remove(at: sourceIndex)
      source.keys.insert(key, at: destinationIndex)
    }
  }

  /*
   * A view sized by sizeThatFits:.
   */
  private final class FittingView: NSView {
    var along: CGFloat = 0

    @objc func sizeThatFits(_ size: NSSize) -> NSSize {
      size.width >= CGFloat.greatestFiniteMagnitude / 2 ? NSSize(width: along, height: size.height) : NSSize(width: size.width, height: along)
    }
  }

  private func makeHarness(
    size: NSSize = NSSize(width: 320, height: 480), configure: (ShadowListKitListView) -> Void = { _ in }
  ) -> ListHarness {
    ListHarness(source: Sections(count: 100), size: size) {
      $0.stickySectionHeaders = true
      configure($0)
    }
  }

  /*
   * The shown section header and footer cells by title.
   */
  private func titleCells(_ list: ShadowListKitListView) -> [String: ShadowListKitListCell] {
    var cells: [String: ShadowListKitListCell] = [:]
    for case let cell as ShadowListKitListCell in list.subviews.first!.subviews where !cell.isHidden && cell.index == NSNotFound {
      if let title = cell.subviews.compactMap({ ($0 as? NSTextField)?.stringValue }).first {
        cells[title] = cell
      }
    }
    return cells
  }

  private func pinned(_ list: ShadowListKitListView) -> ShadowListKitListCell? {
    list.subviews.first!.subviews.compactMap { $0 as? ShadowListKitListCell }.first { !$0.isHidden && $0.layer?.zPosition == 1 }
  }

  private func title(of cell: ShadowListKitListCell?) -> String? {
    cell?.subviews.compactMap { ($0 as? NSTextField)?.stringValue }.first
  }

  /*
   * A view's frame in the list's coordinates.
   */
  private func frame(_ view: NSView, in list: ShadowListKitListView) -> CGRect {
    list.convert(view.frame, from: view.superview)
  }

  func testTheSectionHeaderPinsAtTheTopWhileItsSectionScrolls() throws {
    let harness = makeHarness()
    let list = harness.list
    for offset: CGFloat in [0, 100, 300] {
      list.contentOffset = CGPoint(x: 0, y: offset)
      harness.layout()
      let header = try XCTUnwrap(pinned(list))
      XCTAssertEqual(title(of: header), "Section 0")
      XCTAssertEqual(frame(header, in: list).minY, 0, accuracy: 0.01)
    }
    // The header's own place stays the row's.
    XCTAssertEqual(list.rectForHeader(inSection: 0).minY, 0, accuracy: 0.01)
  }

  func testTheNextHeaderPushesThePinnedOneUp() throws {
    let harness = makeHarness()
    let list = harness.list
    list.contentOffset = CGPoint(x: 0, y: 460)
    harness.layout()
    let cells = titleCells(list)
    let first = try XCTUnwrap(cells["Section 0"])
    let second = try XCTUnwrap(cells["Section 1"])
    XCTAssertEqual(pinned(list), first)
    XCTAssertEqual(frame(first, in: list).minY, -20, accuracy: 0.01)
    XCTAssertEqual(frame(second, in: list).minY, 10, accuracy: 0.01)
    XCTAssertEqual(frame(first, in: list).maxY, frame(second, in: list).minY, accuracy: 0.01)

    // Past it the next header pins and the one it replaced goes back to its row.
    list.contentOffset = CGPoint(x: 0, y: 1000)
    harness.layout()
    let third = try XCTUnwrap(titleCells(list)["Section 2"])
    XCTAssertEqual(pinned(list), third)
    XCTAssertEqual(frame(third, in: list).minY, 0, accuracy: 0.01)
    if let replaced = titleCells(list)["Section 1"] {
      XCTAssertEqual(replaced.layer?.zPosition, 0)
      XCTAssertEqual(frame(replaced, in: list).minY, 470 - 1000, accuracy: 0.01)
    }
  }

  func testAPinnedHeaderCoversTheItemsUnderIt() throws {
    let harness = makeHarness()
    let list = harness.list
    list.contentOffset = CGPoint(x: 0, y: 100)
    harness.layout()
    let header = try XCTUnwrap(pinned(list))
    let container = try XCTUnwrap(header.superview)
    // Item 1 runs from 74 to 118 and shows under the header.
    let under = try XCTUnwrap(list.cellForItem(at: 1))
    XCTAssertGreaterThan(frame(under, in: list).maxY, 10)
    XCTAssertGreaterThan(header.layer!.zPosition, under.layer!.zPosition)
    XCTAssertEqual(container.subviews.contains(header), true)
    list.shadowListKit_moveMouse(to: CGPoint(x: 100, y: 10))
    XCTAssertFalse(under.isHovered)
    list.shadowListKit_mouseDown(at: CGPoint(x: 100, y: 10))
    list.shadowListKit_mouseUp(at: CGPoint(x: 100, y: 10))
    XCTAssertEqual(list.selectedIndices.count, 0)
  }

  func testStickyHeadersWorkHorizontally() throws {
    let harness = makeHarness(size: NSSize(width: 480, height: 320)) { $0.isHorizontal = true }
    let list = harness.list
    list.contentOffset = CGPoint(x: 100, y: 0)
    harness.layout()
    let header = try XCTUnwrap(pinned(list))
    XCTAssertEqual(title(of: header), "Section 0")
    XCTAssertEqual(frame(header, in: list).minX, 0, accuracy: 0.01)
    XCTAssertEqual(frame(header, in: list).width, 30, accuracy: 0.01)

    list.contentOffset = CGPoint(x: 460, y: 0)
    harness.layout()
    XCTAssertEqual(frame(try XCTUnwrap(pinned(list)), in: list).minX, -20, accuracy: 0.01)
  }

  /*
   * Past the start the first header goes with the content. Past the end the last one stays
   * pinned at the top.
   */
  func testStickyHeadersDuringOverscroll() throws {
    let harness = makeHarness()
    let list = harness.list
    list.scrollPhase = .dragging
    list.contentOffset = CGPoint(x: 0, y: -40)
    harness.layout()
    let first = try XCTUnwrap(titleCells(list)["Section 0"])
    XCTAssertNil(pinned(list))
    XCTAssertEqual(frame(first, in: list).minY, 40, accuracy: 0.01)

    let end = list.contentSize.height - list.bounds.height
    list.contentOffset = CGPoint(x: 0, y: end + 60)
    harness.layout()
    let last = try XCTUnwrap(pinned(list))
    XCTAssertEqual(title(of: last), "Section 9")
    XCTAssertEqual(frame(last, in: list).minY, 0, accuracy: 0.01)
    list.scrollPhase = .idle
  }

  func testStickyHeadersFollowADrivenOffset() throws {
    let harness = makeHarness { $0.isScrollDriven = true }
    let list = harness.list
    let host = Host(source: harness.source)
    list.delegate = host
    for offset: CGFloat in [120, 460, 1000, -30] {
      list.contentOffset = CGPoint(x: 0, y: offset)
      harness.layout()
      // The views lag behind their layers until the list rests.
      harness.rest()
      XCTAssertEqual(list.contentOffset.y, offset, accuracy: 0.01)
      switch offset {
      case 120: XCTAssertEqual(frame(try XCTUnwrap(pinned(list)), in: list).minY, 0, accuracy: 0.01)
      case 460: XCTAssertEqual(frame(try XCTUnwrap(pinned(list)), in: list).minY, -20, accuracy: 0.01)
      case 1000: XCTAssertEqual(title(of: pinned(list)), "Section 2")
      default:
        XCTAssertNil(pinned(list))
        XCTAssertEqual(frame(try XCTUnwrap(titleCells(list)["Section 0"]), in: list).minY, 30, accuracy: 0.01)
      }
    }
  }

  func testStickyIndicesPinItems() throws {
    let harness = ListHarness(source: SizedTestSource(count: 100)) { $0.stickyIndices = IndexSet([5, 20]) }
    let list = harness.list
    list.contentOffset = CGPoint(x: 0, y: 400)
    harness.layout()
    let cell = try XCTUnwrap(list.cellForItem(at: 5))
    XCTAssertEqual(frame(cell, in: list).minY, 0, accuracy: 0.01)
    XCTAssertEqual(cell.layer?.zPosition, 1)
    // A click on the pinned item selects it, not the item under it.
    list.shadowListKit_mouseDown(at: CGPoint(x: 100, y: 10))
    list.shadowListKit_mouseUp(at: CGPoint(x: 100, y: 10))
    XCTAssertEqual(Array(list.selectedIndices), [5])

    list.stickyIndices = nil
    harness.layout()
    XCTAssertEqual(cell.layer?.zPosition, 0)
    XCTAssertEqual(frame(cell, in: list).minY, 5 * 44 - 400, accuracy: 0.01)
  }

  /*
   * A held row sits over the pinned header, keeps its own transform, and the header stays
   * pinned while the edge scroll moves the list.
   */
  func testThePinnedHeaderStaysUnderAHeldRow() throws {
    let harness = makeHarness { $0.reorderEnabled = true }
    let list = harness.list
    let host = Host(source: harness.source)
    list.delegate = host
    list.contentOffset = CGPoint(x: 0, y: 100)
    harness.layout()
    let held = try XCTUnwrap(list.cellForItem(at: 4))
    let start = CGPoint(x: 100, y: frame(held, in: list).midY)
    list.shadowListKit_beginDrag(at: start)
    list.shadowListKit_moveDrag(to: CGPoint(x: 100, y: 12))
    harness.layout()
    XCTAssertEqual(list.shadowListKit_heldIndex, 4)
    let header = try XCTUnwrap(pinned(list))
    XCTAssertEqual(frame(header, in: list).minY, 0, accuracy: 0.01)
    XCTAssertEqual(held.layer?.zPosition, 2)
    XCTAssertFalse(CATransform3DIsIdentity(held.layer!.transform))
    let heldFrame = held.layer!.frame

    // The edge scroll at the bottom moves the list under the held row.
    list.shadowListKit_moveDrag(to: CGPoint(x: 100, y: 470))
    list.shadowListKit_runDragFrames(for: 0.5)
    harness.layout()
    XCTAssertGreaterThan(list.contentOffset.y, 100)
    let pinnedNow = try XCTUnwrap(pinned(list))
    XCTAssertLessThanOrEqual(frame(pinnedNow, in: list).minY, 0.01)
    XCTAssertGreaterThan(frame(pinnedNow, in: list).maxY, 0)
    XCTAssertEqual(held.layer?.zPosition, 2)
    XCTAssertNotEqual(held.layer!.frame, heldFrame)

    list.shadowListKit_endDrag()
    harness.layout()
    XCTAssertEqual(try XCTUnwrap(pinned(list)).layer?.zPosition, 1)
    XCTAssertEqual(frame(try XCTUnwrap(pinned(list)), in: list).minY, 0, accuracy: 0.01)
  }

  func testScrollToSectionPutsItsHeaderAtTheTop() throws {
    let harness = makeHarness()
    let list = harness.list
    list.scrollToSection(3, animated: false)
    harness.layout()
    XCTAssertEqual(list.rectForHeader(inSection: 3).minY - list.contentOffset.y, 0, accuracy: 0.01)
    XCTAssertEqual(title(of: pinned(list)), "Section 3")
    XCTAssertEqual(list.sectionForItem(at: list.visibleRange.location), 3)
  }

  func testTitleCellsFollowTheAppearance() throws {
    let harness = makeHarness()
    let source = harness.source as! Sections
    source.footers = true
    harness.list.reloadData()
    harness.window.appearance = NSAppearance(named: .aqua)
    harness.layout()
    let list = harness.list
    let header = try XCTUnwrap(titleCells(list)["Section 0"])
    let footer = try XCTUnwrap(titleCells(list)["10 items"])
    let label = try XCTUnwrap(header.subviews.first as? NSTextField)
    XCTAssertEqual(label.font, NSFont.preferredFont(forTextStyle: .headline))
    XCTAssertEqual(label.textColor, NSColor.secondaryLabelColor)
    XCTAssertEqual((footer.subviews.first as? NSTextField)?.font?.pointSize, NSFont.smallSystemFontSize)
    header.displayIfNeeded()
    let light = try XCTUnwrap(header.layer?.backgroundColor)
    XCTAssertEqual(light.alpha, 1)
    XCTAssertNil(footer.layer?.backgroundColor)

    harness.window.appearance = NSAppearance(named: .darkAqua)
    harness.layout()
    header.displayIfNeeded()
    let dark = try XCTUnwrap(header.layer?.backgroundColor)
    XCTAssertNotEqual(light, dark)
    var expected: CGColor?
    NSAppearance(named: .darkAqua)!.performAsCurrentDrawingAppearance { expected = NSColor.windowBackgroundColor.cgColor }
    XCTAssertEqual(dark, expected)
  }

  func testHeaderAndFooterViewsSitAroundTheItems() throws {
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 10, height: 60))
    let footer = FittingView()
    footer.along = 50
    let source = SizedTestSource(count: 20)
    let harness = ListHarness(source: source) {
      $0.headerView = header
      $0.footerView = footer
    }
    let list = harness.list
    XCTAssertEqual(list.rectForItem(at: 0).minY, 60, accuracy: 0.01)
    XCTAssertEqual(list.contentSize.height, 60 + 20 * 44 + 50, accuracy: 0.01)
    XCTAssertEqual(frame(header, in: list), CGRect(x: 0, y: 0, width: 320, height: 60))
    XCTAssertEqual(header.superview, list.cellForItem(at: 0)?.superview)
    // Both sit under every cell.
    XCTAssertEqual(Set(header.superview!.subviews.prefix(2).map(ObjectIdentifier.init)), [ObjectIdentifier(header), ObjectIdentifier(footer)])

    list.scrollToEnd(animated: false)
    harness.layout()
    XCTAssertEqual(frame(footer, in: list).maxY, list.bounds.height, accuracy: 0.01)
    XCTAssertEqual(frame(footer, in: list).height, 50, accuracy: 0.01)
    XCTAssertEqual(frame(footer, in: list).minY, frame(try XCTUnwrap(list.cellForItem(at: 19)), in: list).maxY, accuracy: 0.01)

    list.scrollToItem(at: 5, viewPosition: 0, animated: false)
    harness.layout()
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 5)), 0, accuracy: 0.01)
    XCTAssertEqual(list.contentOffset.y, 60 + 5 * 44, accuracy: 0.01)

    // A header that grows moves the items and keeps the visible ones in place.
    header.setFrameSize(NSSize(width: 320, height: 100))
    list.needsLayout = true
    harness.layout()
    XCTAssertEqual(list.rectForItem(at: 0).minY, 100, accuracy: 0.01)
    XCTAssertEqual(try XCTUnwrap(harness.screenY(ofItem: 5)), 0, accuracy: 0.01)

    list.headerView = nil
    list.footerView = nil
    harness.layout()
    XCTAssertNil(header.superview)
    XCTAssertNil(footer.superview)
    XCTAssertEqual(list.rectForItem(at: 0).minY, 0, accuracy: 0.01)
    XCTAssertEqual(list.contentSize.height, 20 * 44, accuracy: 0.01)
  }

  func testHeaderViewsWithoutAFrameSizeThemselves() throws {
    let fitting = FittingView()
    fitting.along = 70
    let harness = ListHarness(source: SizedTestSource(count: 20)) { $0.headerView = fitting }
    XCTAssertEqual(harness.list.rectForItem(at: 0).minY, 70, accuracy: 0.01)

    let constrained = NSView()
    constrained.translatesAutoresizingMaskIntoConstraints = false
    constrained.heightAnchor.constraint(equalToConstant: 40).isActive = true
    harness.list.headerView = constrained
    harness.layout()
    XCTAssertEqual(harness.list.rectForItem(at: 0).minY, 40, accuracy: 0.01)
  }

  func testAHorizontalHeaderViewTakesItsWidth() throws {
    let header = NSView(frame: NSRect(x: 0, y: 0, width: 80, height: 10))
    let harness = ListHarness(source: SizedTestSource(count: 20), size: NSSize(width: 480, height: 320)) {
      $0.isHorizontal = true
      $0.headerView = header
    }
    let list = harness.list
    XCTAssertEqual(list.rectForItem(at: 0).minX, 80, accuracy: 0.01)
    XCTAssertEqual(frame(header, in: list), CGRect(x: 0, y: 0, width: 80, height: 320))
  }
}
