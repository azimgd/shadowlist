import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Swipe actions with a trackpad: the reveal and where it rests, the full swipe, clicks, what
 * closes an open item, the mouse wheel, the momentum after a swipe, the menu fallback,
 * accessibility and recycling.
 */
@MainActor
final class SwipeTests: XCTestCase {
  /*
   * Gives every item a Pin action on its leading side and Delete and Archive on its trailing
   * side, except item 7, which has none. Records the actions performed and deletes from its
   * source on Delete when deletes is set.
   */
  private final class Host: NSObject, ShadowListKitListViewDelegate {
    let source: TestSource
    var performed: [String] = []
    var completes = true
    var deletes = false
    var menus = false

    init(source: TestSource) {
      self.source = source
    }

    private func action(_ title: String, _ style: ShadowListKitSwipeActionStyle, _ listView: ShadowListKitListView, _ index: Int)
      -> ShadowListKitSwipeAction
    {
      ShadowListKitSwipeAction(style: style, title: title) { [weak self, weak listView] _, completion in
        guard let self else { return }
        self.performed.append("\(title)-\(index)")
        if title == "Delete", self.deletes, let listView {
          self.source.keys.remove(at: index)
          listView.deleteItems(at: IndexSet(integer: index))
        }
        if self.completes {
          completion(true)
        }
      }
    }

    func listView(_ listView: ShadowListKitListView, leadingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
      index == 7 ? nil : ShadowListKitSwipeActionsConfiguration(actions: [action("Pin", .normal, listView, index)])
    }

    func listView(_ listView: ShadowListKitListView, trailingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
      index == 7
        ? nil
        : ShadowListKitSwipeActionsConfiguration(actions: [
          action("Delete", .destructive, listView, index), action("Archive", .normal, listView, index),
        ])
    }

    func listView(_ listView: ShadowListKitListView, contextMenuForItemAt index: Int) -> NSMenu? {
      menus && index == 2 ? NSMenu(title: "own") : nil
    }

    func listView(_ listView: ShadowListKitListView, canMoveItemAt index: Int) -> Bool { true }

    func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
      source.keys.insert(source.keys.remove(at: sourceIndex), at: destinationIndex)
    }
  }

  /*
   * Stands between the list and its superview and records the wheel events the list passes on.
   */
  private final class WheelRecorder: NSResponder {
    var events: [NSEvent] = []

    override func scrollWheel(with event: NSEvent) {
      events.append(event)
    }
  }

  /*
   * The list holds its delegate weakly. The test keeps it.
   */
  private var delegates: [NSObject] = []

  /*
   * 600 items of 44 points in a 320 by 480 list, with a host that swipes them.
   */
  private func makeHarness(configure: (ShadowListKitListView) -> Void = { _ in }) -> (ListHarness, Host, WheelRecorder) {
    let source = SizedTestSource(count: 600)
    let harness = ListHarness(source: source, configure: configure)
    let host = Host(source: source)
    delegates.append(host)
    harness.list.delegate = host
    let recorder = WheelRecorder()
    recorder.nextResponder = harness.list.nextResponder
    harness.list.nextResponder = recorder
    return (harness, host, recorder)
  }

  private func center(_ list: ShadowListKitListView, _ index: Int) throws -> CGPoint {
    let cell = try XCTUnwrap(list.cellForItem(at: index))
    let frame = list.convert(cell.frame, from: cell.superview)
    return CGPoint(x: frame.midX, y: frame.midY)
  }

  /*
   * A two finger gesture over an item: the fingers land, move by each delta across the axis one
   * event apart, and lift. momentum events follow with the momentum deltas.
   */
  private func swipe(
    _ list: ShadowListKitListView, _ index: Int, _ deltas: [CGFloat], momentum: [CGFloat] = [], horizontal: Bool = false
  ) throws {
    let point = try center(list, index)
    func delta(_ value: CGFloat) -> CGPoint { horizontal ? CGPoint(x: 0, y: value) : CGPoint(x: value, y: 0) }
    list.shadowListKit_swipeWheelDelta(.zero, phase: .began, momentumPhase: [], at: point)
    for value in deltas {
      list.shadowListKit_swipeWheelDelta(delta(value), phase: .changed, momentumPhase: [], at: point)
    }
    list.shadowListKit_swipeWheelDelta(.zero, phase: .ended, momentumPhase: [], at: point)
    for (at, value) in momentum.enumerated() {
      let phase: NSEvent.Phase = at == 0 ? .began : at == momentum.count - 1 ? .ended : .changed
      list.shadowListKit_swipeWheelDelta(delta(value), phase: [], momentumPhase: phase, at: point)
    }
  }

  /*
   * The total width of the shown buttons.
   */
  private func shownButtonsWidth(_ list: ShadowListKitListView) throws -> CGFloat {
    let view = try XCTUnwrap(list.shadowListKit_swipeActionsView)
    return view.subviews.filter { !$0.isHidden }.reduce(0) { $0 + $1.frame.width }
  }

  private func swipeShift(_ cell: ShadowListKitListCell?) -> CGPoint? {
    (cell?.layer?.animation(forKey: "shadowListKitSwipe") as? CABasicAnimation)?.toValue.map { ($0 as! NSValue).pointValue }
  }

  private func spinMainQueue() {
    RunLoop.main.run(until: Date().addingTimeInterval(0.05))
  }

  func testAPartialSwipePastHalfTheButtonsRestsOpen() throws {
    let (harness, host, recorder) = makeHarness()
    let list = harness.list
    let frame = try XCTUnwrap(list.cellForItem(at: 3)).frame
    try swipe(list, 3, Array(repeating: -2, count: 50))
    XCTAssertEqual(list.shadowListKit_swipedIndex, 3)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    let width = try shownButtonsWidth(list)
    XCTAssertGreaterThan(width, 140)
    XCTAssertEqual(list.shadowListKit_swipeOffset, -width, accuracy: 0.5)
    // The cell shows moved through its own animation. Its frame and transform stay the list's.
    XCTAssertEqual(swipeShift(list.cellForItem(at: 3))?.x ?? 0, -width, accuracy: 0.5)
    XCTAssertEqual(list.cellForItem(at: 3)?.frame, frame)
    XCTAssertTrue(CATransform3DIsIdentity(try XCTUnwrap(list.cellForItem(at: 3)?.layer).transform))
    XCTAssertEqual(host.performed, [])
    XCTAssertEqual(recorder.events.count, 0)
  }

  func testAShortSlowSwipeClosesAgain() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    try swipe(list, 3, Array(repeating: -2, count: 10))
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertNil(list.shadowListKit_swipeActionsView)
    XCTAssertNil(swipeShift(list.cellForItem(at: 3)))
  }

  func testAFlingOpensAShortSwipeAndAFlingBackClosesIt() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    try swipe(list, 3, [-12, -12, -12])
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, 3)
    XCTAssertEqual(list.shadowListKit_swipeOffset, -(try shownButtonsWidth(list)), accuracy: 0.5)
    try swipe(list, 3, [12, 12, 12])
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
  }

  func testASwipeTowardTheTrailingSideShowsTheLeadingActions() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    try swipe(list, 2, Array(repeating: 2, count: 50))
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, 2)
    XCTAssertGreaterThan(list.shadowListKit_swipeOffset, 0)
    let titles = try XCTUnwrap(list.shadowListKit_swipeActionsView).subviews.compactMap { $0.isHidden ? nil : ($0 as? NSButton)?.title }
    XCTAssertEqual(titles, ["Pin"])
  }

  func testAFullSwipePerformsTheFirstActionAndClosesOnItsCompletion() throws {
    let (harness, host, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_swipeItem(at: 4, distance: -300, velocity: 0)
    XCTAssertEqual(host.performed, ["Delete-4"])
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipeOffset, -320, accuracy: 0.5)
    spinMainQueue()
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
  }

  func testAFullSwipeThatDeletesTheItemRecyclesItsCellWithoutAFade() throws {
    let (harness, host, _) = makeHarness { $0.animatesChanges = true }
    let list = harness.list
    host.deletes = true
    let cell = try XCTUnwrap(list.cellForItem(at: 4))
    list.shadowListKit_swipeItem(at: 4, distance: -300, velocity: 0)
    harness.layout()
    XCTAssertEqual(host.performed, ["Delete-4"])
    XCTAssertTrue(cell.isHidden)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertNil(cell.layer?.animation(forKey: "shadowListKitDelete"))
  }

  func testAFullSwipeWithoutItsSwitchRubberBandsAndRestsOpen() throws {
    final class NoFull: NSObject, ShadowListKitListViewDelegate {
      func listView(_ listView: ShadowListKitListView, trailingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
        let configuration = ShadowListKitSwipeActionsConfiguration(actions: [ShadowListKitSwipeAction(style: .normal, title: "Flag") { _, _ in }])
        configuration.performsFirstActionWithFullSwipe = false
        return configuration
      }
    }
    let (harness, _, _) = makeHarness()
    let host = NoFull()
    delegates.append(host)
    harness.list.delegate = host
    harness.list.shadowListKit_swipeItem(at: 1, distance: -300, velocity: 0)
    harness.list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(harness.list.shadowListKit_swipeOffset, -(try shownButtonsWidth(harness.list)), accuracy: 0.5)
  }

  func testClickingAnActionPerformsItAndCloses() throws {
    let (harness, host, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    let point = try center(list, 3)
    // The first action sits at the outer edge.
    let edge = CGPoint(x: list.bounds.width - 10, y: point.y)
    list.shadowListKit_mouseDown(at: edge)
    list.shadowListKit_mouseUp(at: edge)
    XCTAssertEqual(host.performed, ["Delete-3"])
    spinMainQueue()
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertEqual(Array(list.selectedIndices), [])
  }

  func testAnActionThatDoesNotCompleteKeepsTheItemOpen() throws {
    let (harness, host, _) = makeHarness()
    let list = harness.list
    host.completes = false
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    let point = try center(list, 3)
    let inner = CGPoint(x: list.bounds.width + list.shadowListKit_swipeOffset + 10, y: point.y)
    list.shadowListKit_mouseDown(at: inner)
    list.shadowListKit_mouseUp(at: inner)
    XCTAssertEqual(host.performed, ["Archive-3"])
    spinMainQueue()
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, 3)
  }

  func testAClickElsewhereOnlyCloses() throws {
    let (harness, host, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    // Clicks over the content are the list's while an item is open.
    let point = try center(list, 6)
    let container = try XCTUnwrap(list.superview)
    XCTAssertTrue(container.hitTest(list.convert(point, to: container)) === list)
    list.shadowListKit_clickItem(at: 6, modifierFlags: [], clickCount: 1)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertEqual(Array(list.selectedIndices), [])
    XCTAssertEqual(host.performed, [])
    // Closed, the click selects again.
    list.shadowListKit_clickItem(at: 6, modifierFlags: [], clickCount: 1)
    XCTAssertEqual(Array(list.selectedIndices), [6])
    XCTAssertTrue(container.hitTest(list.convert(point, to: container)) === list.cellForItem(at: 6))
  }

  func testScrollingCloses() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    list.shadowListKit_scrollWheelDelta(.zero, phase: .began, momentumPhase: [])
    list.shadowListKit_scrollWheelDelta(CGPoint(x: 0, y: -20), phase: .changed, momentumPhase: [])
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
  }

  func testEscapeCloses() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_clickItem(at: 1, modifierFlags: [], clickCount: 1)
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    list.shadowListKit_pressKey(Key.escape.characters, keyCode: Key.escape.code, modifierFlags: [])
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    // The first Escape only closed the item.
    XCTAssertEqual(Array(list.selectedIndices), [1])
  }

  func testPickingUpARowCloses() throws {
    let (harness, _, _) = makeHarness { $0.reorderEnabled = true }
    let list = harness.list
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    list.shadowListKit_beginDrag(at: try center(list, 6))
    XCTAssertEqual(list.shadowListKit_heldIndex, 6)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    list.shadowListKit_endDrag()
  }

  func testCloseSwipeActionsClosesAndADataChangeToo() throws {
    let (harness, host, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    list.closeSwipeActions(animated: false)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    host.source.keys.insert("new", at: 0)
    list.insertItems(at: IndexSet(integer: 0))
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertNil(list.shadowListKit_swipeActionsView)
  }

  func testAMouseWheelNeverSwipes() throws {
    let (harness, _, recorder) = makeHarness()
    let list = harness.list
    let point = try center(list, 3)
    for _ in 0..<5 {
      list.shadowListKit_swipeWheelDelta(CGPoint(x: -20, y: 0), phase: [], momentumPhase: [], at: point)
    }
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertNil(list.shadowListKit_swipeActionsView)
    XCTAssertEqual(recorder.events.count, 5)
  }

  func testTheListTakesTheMomentumAfterASwipe() throws {
    let (harness, _, recorder) = makeHarness()
    let list = harness.list
    try swipe(list, 3, [-12, -12, -12], momentum: [-8, -6, -4, -2])
    XCTAssertEqual(recorder.events.count, 0)
    XCTAssertEqual(list.shadowListKit_swipedIndex, 3)
    // The next gesture along the axis scrolls.
    list.shadowListKit_runSwipeFrames(for: 0.5)
    let offset = list.contentOffset.y
    list.shadowListKit_scrollWheelDelta(.zero, phase: .began, momentumPhase: [])
    list.shadowListKit_scrollWheelDelta(CGPoint(x: 0, y: -30), phase: .changed, momentumPhase: [])
    list.shadowListKit_scrollWheelDelta(.zero, phase: .ended, momentumPhase: [])
    XCTAssertGreaterThan(list.contentOffset.y, offset)
  }

  func testASwipeOnAnItemWithoutActionsGoesToTheNextResponder() throws {
    let (harness, _, recorder) = makeHarness()
    let list = harness.list
    try swipe(list, 7, [-12, -12, -12], momentum: [-8, -2])
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    // Every event from the one that went across the axis, momentum included.
    XCTAssertEqual(recorder.events.count, 6)
  }

  func testAListWithoutSwipeActionsLeavesTheGestureAndTheMenu() throws {
    let harness = ListHarness(source: SizedTestSource(count: 100))
    let list = harness.list
    let recorder = WheelRecorder()
    recorder.nextResponder = list.nextResponder
    list.nextResponder = recorder
    try swipe(list, 3, [-12, -12, -12])
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertNil(list.shadowListKit_swipeActionsView)
    XCTAssertEqual(recorder.events.count, 4)
    XCTAssertNil(list.shadowListKit_menuForItem(at: 3, modifierFlags: []))
    let row = try XCTUnwrap(list.accessibilityRows()?[3] as? NSAccessibilityElement)
    XCTAssertEqual(row.accessibilityCustomActions()?.count ?? 0, 0)
  }

  func testADrivenListNeverSwipes() throws {
    let (harness, _, _) = makeHarness { $0.scrollDriven = true }
    try swipe(harness.list, 3, [-12, -12, -12])
    XCTAssertEqual(harness.list.shadowListKit_swipedIndex, NSNotFound)
  }

  func testAHorizontalListSwipesAcrossItsAxis() throws {
    let (harness, _, _) = makeHarness { $0.isHorizontal = true }
    let list = harness.list
    try swipe(list, 2, Array(repeating: -2, count: 60), horizontal: true)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, 2)
    XCTAssertEqual(swipeShift(list.cellForItem(at: 2))?.x ?? -1, 0)
    XCTAssertLessThan(swipeShift(list.cellForItem(at: 2))?.y ?? 0, 0)
  }

  func testItemsWithoutAMenuShowTheirSwipeActions() throws {
    let (harness, host, _) = makeHarness()
    let list = harness.list
    let menu = try XCTUnwrap(list.shadowListKit_menuForItem(at: 3, modifierFlags: []))
    XCTAssertEqual(menu.items.map { $0.isSeparatorItem ? "-" : $0.title }, ["Delete", "Archive", "-", "Pin"])
    let delete = menu.items[0]
    let color = delete.attributedTitle?.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? NSColor
    XCTAssertEqual(color, NSColor.systemRed)
    XCTAssertNil(menu.items[1].attributedTitle?.attribute(.foregroundColor, at: 0, effectiveRange: nil))
    XCTAssertEqual(list.clickedIndex, 3)
    menu.performActionForItem(at: 1)
    XCTAssertEqual(host.performed, ["Archive-3"])
    NotificationCenter.default.post(name: NSMenu.didEndTrackingNotification, object: menu)
    // The delegate's own menu wins, and an item without either gets none.
    host.menus = true
    XCTAssertEqual(list.shadowListKit_menuForItem(at: 2, modifierFlags: [])?.title, "own")
    XCTAssertNil(list.shadowListKit_menuForItem(at: 7, modifierFlags: []))
  }

  func testARightClickClosesAnOpenItem() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    let menu = try XCTUnwrap(list.shadowListKit_menuForItem(at: 5, modifierFlags: []))
    list.shadowListKit_runSwipeFrames(for: 0.5)
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    NotificationCenter.default.post(name: NSMenu.didEndTrackingNotification, object: menu)
  }

  func testAccessibilityRunsTheSwipeActions() throws {
    let (harness, host, _) = makeHarness { $0.reorderEnabled = true }
    let list = harness.list
    let row = try XCTUnwrap(list.accessibilityRows()?[3] as? NSAccessibilityElement)
    let actions = try XCTUnwrap(row.accessibilityCustomActions())
    XCTAssertEqual(actions.map(\.name), ["Delete", "Archive", "Pin", "Move up", "Move down"])
    XCTAssertTrue(actions[2].handler?() ?? false)
    XCTAssertEqual(host.performed, ["Pin-3"])
    let none = try XCTUnwrap(list.accessibilityRows()?[7] as? NSAccessibilityElement)
    XCTAssertEqual(none.accessibilityCustomActions()?.map(\.name), ["Move up", "Move down"])
  }

  func testAccessibilityHitsTheButtonsOfAnOpenItem() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    list.shadowListKit_swipeItem(at: 3, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    let point = try center(list, 3)
    let local = CGPoint(x: list.bounds.width - 10, y: point.y)
    let screen = harness.window.convertPoint(toScreen: list.convert(local, to: nil))
    let hit = list.accessibilityHitTest(screen)
    // AppKit answers for a button through its cell.
    XCTAssertEqual((hit as? NSButtonCell)?.title ?? (hit as? NSButton)?.title, "Delete")
  }

  func testARecycledCellComesBackClosed() throws {
    let (harness, _, _) = makeHarness()
    let list = harness.list
    let cell = try XCTUnwrap(list.cellForItem(at: 1))
    list.shadowListKit_swipeItem(at: 1, distance: -100, velocity: 0)
    list.shadowListKit_runSwipeFrames(for: 0.5)
    list.contentOffset = CGPoint(x: 0, y: 8000)
    harness.layout()
    XCTAssertEqual(list.shadowListKit_swipedIndex, NSNotFound)
    XCTAssertNil(list.shadowListKit_swipeActionsView)
    XCTAssertNil(swipeShift(cell))
    list.contentOffset = .zero
    harness.layout()
    XCTAssertNil(swipeShift(list.cellForItem(at: 1)))
  }
}
