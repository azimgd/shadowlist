import ShadowListKit
import ShadowListUtils
import UIKit

/*
 * -SLRoute Board -SLScenario board-edge runs the existing drag frame loop in the application's
 * real scene. Its display link records the outer host's offsets through edge scroll and snap.
 */
@MainActor
final class BoardScenario: NSObject {
  private let source: ShadowListKitListView
  private let outer: ShadowListKitListView
  private let cell: ShadowListKitListCell
  private let began = CACurrentMediaTime()
  private var link: CADisplayLink?
  private var released = false
  private var releaseOffset: CGFloat = 0
  private var offsets: [CGFloat] = []

  static func start<Item>(_ board: ShadowListUtilsBoardList<Item>) {
    guard UserDefaults.standard.string(forKey: "SLScenario") == "board-edge" else { return }
    board.layoutIfNeeded()
    guard let outer = board.subviews.first as? ShadowListKitListView, let window = board.window else {
      preconditionFailure("The board-edge scenario needs an attached board")
    }
    outer.layoutIfNeeded()
    guard let column = outer.cellForItem(at: 0), let source = list(in: column) else {
      preconditionFailure("The board-edge source column must be mounted")
    }
    source.layoutIfNeeded()
    guard let cell = source.cellForItem(at: 0) else { preconditionFailure("The board-edge source must mount its first item") }
    _ = BoardScenario(source: source, outer: outer, cell: cell, window: window)
  }

  private static func list(in view: UIView) -> ShadowListKitListView? {
    if let list = view as? ShadowListKitListView, list.dragListId?.isEmpty == false { return list }
    for child in view.subviews {
      if let list = list(in: child) { return list }
    }
    return nil
  }

  private init(source: ShadowListKitListView, outer: ShadowListKitListView, cell: ShadowListKitListCell, window: UIWindow) {
    self.source = source
    self.outer = outer
    self.cell = cell
    super.init()
    let picked = CGPoint(x: cell.frame.midX, y: cell.frame.midY)
    let y = source.convert(picked, to: window).y
    let edge = outer.convert(CGPoint(x: outer.bounds.maxX - 3, y: 0), to: window).x
    source.shadowListKit_beginDrag(at: picked)
    source.shadowListKit_moveDrag(to: source.convert(CGPoint(x: edge, y: y), from: window))
    let link = CADisplayLink(target: self, selector: #selector(tick))
    self.link = link
    link.add(to: .main, forMode: .common)
  }

  @objc private func tick() {
    let offset = outer.contentOffset.x + outer.contentInset.left
    offsets.append(offset)
    if !released, offset > 12 {
      released = true
      releaseOffset = offset
      source.reorderEnabled = false
    } else if released, offset == 0 {
      finish(snapped: true)
      return
    }
    if CACurrentMediaTime() - began > 3 { finish(snapped: false) }
  }

  private func finish(snapped: Bool) {
    link?.invalidate()
    link = nil
    let checks = [
      "edgeScrolled": released,
      "snapped": snapped,
      "sourceAttached": source.window != nil,
      "sameSourceCell": source.cellForItem(at: 0) === cell,
      "sourceColumnMounted": outer.cellForItem(at: 0) != nil,
    ]
    let result: [String: Any] = [
      "scenario": "board-edge", "checks": checks, "releaseOffset": releaseOffset,
      "finalOffset": outer.contentOffset.x + outer.contentInset.left,
      "offsets": offsets.map(Double.init),
    ]
    let data = try! JSONSerialization.data(withJSONObject: result, options: [.sortedKeys])
    print("[SLSCENARIO] \(String(data: data, encoding: .utf8)!)")
    fflush(stdout)
    let passed = checks.values.allSatisfy { $0 }
    if UserDefaults.standard.bool(forKey: "SLBenchExit") { exit(passed ? 0 : 1) }
    precondition(passed, "The board-edge scenario failed: \(checks)")
  }
}
