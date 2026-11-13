import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class BoardTests: XCTestCase {
  private typealias Column = ShadowListUtilsBoardColumn<String>
  private typealias Board = ShadowListUtilsBoardList<String>
  private var columns: [Column] { [Column(key: "todo", title: "To do", items: ["a", "b"]), Column(key: "done", title: "Done", items: [])] }

  private func makeBoard() -> Board {
    let board = Board(theme: .light, keyExtractor: { $0 }, rowForItem: { ShadowListUtilsBoardCard(key: $0, title: $0, theme: .light) })
    board.columnWidth = 220
    board.sidePadding = 12
    board.columns = columns
    return board
  }

  private func hostBoard(_ board: Board, width: CGFloat = 600) -> UIWindow {
    let frame = CGRect(x: 0, y: 0, width: width, height: 600)
    let scene = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first
    let window = scene.map { UIWindow(windowScene: $0) } ?? UIWindow(frame: frame)
    window.frame = frame
    let controller = UIViewController()
    window.rootViewController = controller
    controller.view.frame = window.bounds
    board.frame = controller.view.bounds
    controller.view.addSubview(board)
    window.makeKeyAndVisible()
    controller.view.layoutIfNeeded()
    board.layoutIfNeeded()
    for key in columns.map(\.key) { board.list(forColumn: key)?.layoutIfNeeded() }
    return window
  }

  func testSharedBoardMoveVectorsPassThroughTheNativeBridge() throws {
    for entry in try Vectors.cases("board-move.json") {
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      let output = try XCTUnwrap(entry["output"] as? [String: Any])
      let keys = try XCTUnwrap(input["columns"] as? [[String]])
      let expected = output["plan"] as? [String: Int]
      let item = try XCTUnwrap(input["itemKey"] as? String)
      let destination = try XCTUnwrap(input["destinationColumn"] as? Int)
      let destinationKey = try XCTUnwrap(input["destinationKey"] as? String)
      let index = try XCTUnwrap(input["destinationIndex"] as? Int)
      let plan = ShadowListKitBoardMovePlan.plan(columns: keys, itemKey: item,
        destinationColumn: destination < 0 ? UInt(NSNotFound) : UInt(destination), destinationKey: destinationKey,
        destinationIndex: index < 0 ? UInt(NSNotFound) : UInt(index))
      if let expected {
        func native(_ field: String) -> UInt { expected[field]! < 0 ? UInt(NSNotFound) : UInt(expected[field]!) }
        XCTAssertEqual(plan.sourceColumn, native("sourceColumn"))
        XCTAssertEqual(plan.sourceIndex, native("sourceIndex"))
        XCTAssertEqual(plan.destinationColumn, native("destinationColumn"))
        XCTAssertEqual(plan.destinationIndex, native("destinationIndex"))
      } else {
        XCTAssertTrue(plan.isEmpty)
      }
      if destination >= 0 && destination < keys.count {
        let columns = keys.enumerated().map { Column(key: String($0.offset), title: String($0.offset), items: $0.element) }
        let move = ShadowListUtilsBoardMoveInfo(item: item, sourceColumnKey: "source", destinationColumnKey: String(destination), destinationKey: destinationKey, destinationIndex: index)
        let moved = shadowListUtilsMoveBoardItem(columns, move: move, keyExtractor: { $0 })
        XCTAssertEqual(moved.map(\.items), try XCTUnwrap(output["columns"] as? [[String]]))
      }
    }
  }

  func testMovesAreControlledAndTheApplicationCanAcceptOrReject() throws {
    let board = makeBoard()
    var requested: ShadowListUtilsBoardMoveInfo<String>?
    board.onMoveItem = { requested = $0 }
    let window = hostBoard(board)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "todo"))
    let target = try XCTUnwrap(board.list(forColumn: "done"))
    source.delegate?.listView?(source, moveItemAt: 0, toListId: target.dragListId!, destinationIndex: 0, destinationKey: "")
    XCTAssertEqual(requested?.destinationColumnKey, "done")
    XCTAssertEqual(board.columns[0].items, ["a", "b"])
    XCTAssertTrue(board.columns[1].items.isEmpty)
    board.onMoveItem = { move in board.columns = shadowListUtilsMoveBoardItem(board.columns, move: move, keyExtractor: { $0 }) }
    source.delegate?.listView?(source, moveItemAt: 0, toListId: target.dragListId!, destinationIndex: 0, destinationKey: "")
    XCTAssertEqual(board.columns.map(\.items), [["b"], ["a"]])
    XCTAssertEqual(source.rows.map(\.key), ["b"])
    XCTAssertEqual(target.rows.map(\.key), ["a"])
  }

  func testColumnViewsPreserveIdentityAndStayIsolatedBetweenBoards() throws {
    let first = makeBoard()
    let second = makeBoard()
    let list = try XCTUnwrap(first.list(forColumn: "todo"))
    first.columns = columns
    XCTAssertTrue(first.list(forColumn: "todo") === list)
    XCTAssertNotEqual(list.dragBoardId, second.list(forColumn: "todo")?.dragBoardId)
    XCTAssertFalse(list.reorderEnabled)
    first.onMoveItem = { _ in }
    XCTAssertTrue(list.reorderEnabled)
    first.columns = []
    XCTAssertNil(first.list(forColumn: "todo"))
  }

  func testCustomNilHeaderAndFooterStayOutsideTheDraggableRows() throws {
    let board = makeBoard()
    board.renderColumnHeader = { _ in nil }
    board.renderColumnFooter = { _ in UIView(frame: CGRect(x: 0, y: 0, width: 1, height: 44)) }
    let window = hostBoard(board)
    defer { window.isHidden = true }
    let list = try XCTUnwrap(board.list(forColumn: "todo"))
    XCTAssertEqual(list.frame.minY, 0)
    XCTAssertEqual(list.rows.count, 2)
    XCTAssertLessThan(list.frame.height, board.bounds.height)
  }

  func testARealCrossColumnDragAppliesOneMove() throws {
    let board = makeBoard()
    var moves = 0
    board.onMoveItem = { move in moves += 1; board.columns = shadowListUtilsMoveBoardItem(board.columns, move: move, keyExtractor: { $0 }) }
    let window = hostBoard(board)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "todo"))
    let target = try XCTUnwrap(board.list(forColumn: "done"))
    let cell = try XCTUnwrap(source.cellForItem(at: 0))
    let start = CGPoint(x: cell.frame.midX, y: cell.frame.midY)
    source.shadowListKit_beginDrag(at: start)
    let finish = target.convert(CGPoint(x: 100, y: 80), to: source)
    source.shadowListKit_moveDrag(to: finish)
    source.shadowListKit_endDrag()
    XCTAssertEqual(moves, 1)
    XCTAssertEqual(board.columns.map(\.items), [["b"], ["a"]])
  }

  func testARejectedDragAndACancelledDragLeaveTheNextMoveUsable() throws {
    let board = makeBoard()
    var requests = 0
    board.onMoveItem = { _ in requests += 1 }
    let window = hostBoard(board)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "todo"))
    let target = try XCTUnwrap(board.list(forColumn: "done"))
    func liftAcross() throws {
      let cell = try XCTUnwrap(source.cellForItem(at: 0))
      source.shadowListKit_beginDrag(at: CGPoint(x: cell.frame.midX, y: cell.frame.midY))
      source.shadowListKit_moveDrag(to: target.convert(CGPoint(x: 100, y: 80), to: source))
    }
    try liftAcross()
    source.shadowListKit_endDrag()
    XCTAssertEqual(requests, 1)
    XCTAssertEqual(board.columns.map(\.items), [["a", "b"], []])
    try liftAcross()
    source.reorderEnabled = false
    source.shadowListKit_endDrag()
    XCTAssertEqual(requests, 1)
    board.onMoveItem = { move in
      requests += 1
      board.columns = shadowListUtilsMoveBoardItem(board.columns, move: move, keyExtractor: { $0 })
    }
    try liftAcross()
    source.shadowListKit_endDrag()
    XCTAssertEqual(requests, 2)
    XCTAssertEqual(board.columns.map(\.items), [["b"], ["a"]])
  }

  func testDeletingTheSourceItemDuringAFlightCancelsIt() throws {
    let board = makeBoard()
    var requests = 0
    board.onMoveItem = { _ in requests += 1 }
    let window = hostBoard(board)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "todo"))
    let target = try XCTUnwrap(board.list(forColumn: "done"))
    let cell = try XCTUnwrap(source.cellForItem(at: 0))
    source.shadowListKit_beginDrag(at: CGPoint(x: cell.frame.midX, y: cell.frame.midY))
    source.shadowListKit_moveDrag(to: target.convert(CGPoint(x: 100, y: 80), to: source))
    var next = board.columns
    next[0].items = ["b"]
    board.columns = next
    source.shadowListKit_endDrag()
    XCTAssertEqual(requests, 0)
    XCTAssertEqual(board.columns.map(\.items), [["b"], []])
  }

  func testNeverAttachedBoardMembersAreNotKeptAlive() {
    weak var member: ShadowListUtilsList?
    autoreleasepool {
      let board = makeBoard()
      member = board.list(forColumn: "todo")
      XCTAssertNotNil(member)
    }
    XCTAssertNil(member)
  }

  func testAccessibilityNamesTheColumnAndAppendsThroughControlledMoves() throws {
    let board = makeBoard()
    var state = board.columns
    state[1].items = ["z"]
    board.columns = state
    var requests = 0
    board.onMoveItem = { _ in requests += 1 }
    let window = hostBoard(board)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "todo"))
    let cell = try XCTUnwrap(source.cellForItem(at: 0))
    let action = try XCTUnwrap(cell.accessibilityCustomActions?.first(where: { $0.name == "Move to Done" }))
    XCTAssertEqual(action.actionHandler?(action), true)
    XCTAssertEqual(requests, 1)
    XCTAssertEqual(board.columns.map(\.items), [["a", "b"], ["z"]])
    board.onMoveItem = { move in board.columns = shadowListUtilsMoveBoardItem(board.columns, move: move, keyExtractor: { $0 }) }
    let accepted = try XCTUnwrap(cell.accessibilityCustomActions?.first(where: { $0.name == "Move to Done" }))
    XCTAssertEqual(accepted.actionHandler?(accepted), true)
    XCTAssertEqual(board.columns.map(\.items), [["b"], ["z", "a"]])
  }

  func testOuterKitSnapsToColumnsWithoutDetachingTheirLists() throws {
    let board = makeBoard()
    board.columns = (0..<5).map { Column(key: "c\($0)", title: "Column \($0)", items: ["\($0)"]) }
    let window = hostBoard(board, width: 300)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "c0"))
    let outer = try XCTUnwrap(board.subviews.first as? ShadowListKitListView)
    XCTAssertTrue(outer.snapToItem)
    XCTAssertTrue(outer.isHorizontal)
    outer.scrollToItem(at: 2, viewPosition: 0, animated: false)
    outer.layoutIfNeeded()
    XCTAssertEqual(outer.contentOffset.x + outer.contentInset.left, outer.rectForItem(at: 2).minX, accuracy: 0.5)
    XCTAssertNotNil(outer.cellForItem(at: 0))
    XCTAssertNotNil(outer.cellForItem(at: 4))
    XCTAssertTrue(board.list(forColumn: "c0") === source)
    XCTAssertNotNil(source.window)
  }

  // UIScrollView animation delivery is checked by the real-app board-edge scenario. The
  // logic host verifies the same shared target and the source's cleanup and lifetime.
  func testOuterEdgeScrollKeepsTheSourceAttachedAndRequestsTheSharedSnap() async throws {
    let board = makeBoard()
    board.columns = (0..<5).map { Column(key: "c\($0)", title: "Column \($0)", items: ["\($0)"]) }
    board.onMoveItem = { _ in }
    let window = hostBoard(board, width: 300)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "c0"))
    let outer = try XCTUnwrap(board.subviews.first as? ShadowListKitListView)
    let cell = try XCTUnwrap(source.cellForItem(at: 0))
    let scrolled = expectation(description: "The existing drag display link scrolls the outer kit")
    var sawScroll = false
    let scrolling = outer.observe(\.contentOffset, options: [.new]) { list, _ in
      if !sawScroll, list.contentOffset.x + list.contentInset.left > 12 { sawScroll = true; scrolled.fulfill() }
    }
    source.shadowListKit_beginDrag(at: CGPoint(x: cell.frame.midX, y: cell.frame.midY))
    source.shadowListKit_moveDrag(to: source.convert(CGPoint(x: 295, y: 160), from: window))
    await fulfillment(of: [scrolled], timeout: 2)
    scrolling.invalidate()
    let offset = outer.contentOffset.x + outer.contentInset.left
    XCTAssertEqual(outer.shadowListKit_nearestSnapOffset(offset), 0, accuracy: 0.01)
    source.reorderEnabled = false
    XCTAssertNotNil(source.window)
    XCTAssertNotNil(outer.cellForItem(at: 0))
    XCTAssertTrue(source.cellForItem(at: 0) === cell)
  }

  func testRemovingBothColumnsInTheDropCallbackCancelsTheFlightSafely() throws {
    let board = makeBoard()
    var moves = 0
    board.onMoveItem = { _ in moves += 1; board.columns = [] }
    let window = hostBoard(board)
    defer { window.isHidden = true }
    let source = try XCTUnwrap(board.list(forColumn: "todo"))
    let target = try XCTUnwrap(board.list(forColumn: "done"))
    let cell = try XCTUnwrap(source.cellForItem(at: 0))
    source.shadowListKit_beginDrag(at: CGPoint(x: cell.frame.midX, y: cell.frame.midY))
    source.shadowListKit_moveDrag(to: target.convert(CGPoint(x: 100, y: 80), to: source))
    source.shadowListKit_endDrag()
    XCTAssertEqual(moves, 1)
    XCTAssertTrue(board.columns.isEmpty)
    XCTAssertNil(source.superview?.window)
    XCTAssertNil(target.superview?.window)
  }
}
