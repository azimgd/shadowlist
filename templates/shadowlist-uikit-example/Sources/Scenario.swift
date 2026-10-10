import ShadowListKit
import UIKit

/*
 * Scripted correctness checks, started with -SLScenario <name> on a list screen. Each logs one
 * [SLSCENARIO] JSON line and quits with -SLBenchExit 1.
 *
 *   prepend  scroll into the list, add rows at the start, measure how far visible rows moved
 *   append   same with rows added at the end
 *   jump     scrollToIndex to a far row, measure where it lands against the top of the viewport
 *   cost     main thread time of one data update plus its layout, see measureUpdateCost
 *   animate  Feed with animatesChanges: remove a visible row and add one, log the animation
 *   a11y     what VoiceOver sees: element count, a far row, a page scroll
 */
enum Scenario {
  static func start(_ screen: ListScreen) {
    guard let name = UserDefaults.standard.string(forKey: "SLScenario") else { return }
    let list = screen.list!
    if name == "animate" || name == "a11y", let kitList = list.view as? ShadowListKitListView {
      after(1.5) { name == "animate" ? checkAnimation(screen, kitList) : checkAccessibility(kitList) }
      return
    }
    if name == "cost" {
      // The background layout prefetch finishes first. It would compete with the timed updates.
      after(2) { list.cache.whenIdle { measureUpdateCost(screen) } }
      return
    }
    after(1.5) {
      let scroll = list.view
      let inset = scroll.adjustedContentInset
      // Rest in the middle of the loaded content, away from both ends.
      let middle = max(-inset.top, min(4000, (scroll.contentSize.height - scroll.bounds.height) / 2))
      if name != "jump" {
        scroll.setContentOffset(CGPoint(x: 0, y: middle), animated: false)
      }
      after(1.0) {
        let before = visibleRows(scroll)
        switch name {
        case "prepend": screen.prependRows()
        case "append": screen.appendRows()
        case "jump": list.scrollToIndex(list.rows.count * 3 / 4, animated: false)
        default: break
        }
        after(1.0) {
          var result: [String: Any] = ["scenario": name, "engine": screen.engine.rawValue, "screen": String(describing: type(of: screen))]
          if name == "jump" {
            let target = list.rows[list.rows.count * 3 / 4].key
            let rows = visibleRows(scroll)
            let top = scroll.convert(CGPoint(x: 0, y: scroll.contentOffset.y + inset.top), to: nil).y
            result["landedOffset"] = rows[target].map { Double($0 - top) } ?? NSNull()
            result["targetVisible"] = rows[target] != nil
          } else {
            let after = visibleRows(scroll)
            var shift: CGFloat = 0
            var kept = 0
            for (key, y) in before {
              if let moved = after[key] {
                shift = max(shift, abs(moved - y))
                kept += 1
              }
            }
            result["maxShift"] = Double(shift)
            result["visibleBefore"] = before.count
            result["stillVisible"] = kept
          }
          let data = try! JSONSerialization.data(withJSONObject: result, options: .sortedKeys)
          let line = String(data: data, encoding: .utf8)!
          print("[SLSCENARIO] \(line)")
          fflush(stdout)
          if UserDefaults.standard.bool(forKey: "SLBenchExit") {
            exit(0)
          }
        }
      }
    }
  }

  /*
   * Main thread time of one data update plus the layout it causes: prepends then appends
   * while resting mid list, with the layout prefetch paused. Reports medians in milliseconds.
   * prependMs and appendMs count the list's work, the update call plus a synchronous layout.
   * The *TotalMs fields add the screen building its rows, which is the same for every engine.
   * -SLCostRuns sets the updates of each kind, default 15.
   */
  private static func measureUpdateCost(_ screen: ListScreen) {
    let list = screen.list!
    let scroll = list.view
    let runs = max(1, UserDefaults.standard.integer(forKey: "SLCostRuns") > 0 ? UserDefaults.standard.integer(forKey: "SLCostRuns") : 15)
    scroll.setContentOffset(CGPoint(x: 0, y: max(0, (scroll.contentSize.height - scroll.bounds.height) / 2)), animated: false)
    scroll.layoutIfNeeded()
    func timed(_ work: () -> Void) -> (list: Double, total: Double) {
      let start = CACurrentMediaTime()
      work()
      let layoutStart = CACurrentMediaTime()
      scroll.layoutIfNeeded()
      let end = CACurrentMediaTime()
      return ((list.lastUpdateSeconds + end - layoutStart) * 1000, (end - start) * 1000)
    }
    list.cache.prefetchPaused = true
    let prepends = (0..<runs).map { _ in timed { screen.prependRowsNow() } }
    let appends = (0..<runs).map { _ in timed { screen.appendRowsNow() } }
    list.cache.prefetchPaused = false
    func median(_ values: [Double]) -> Double { values.sorted()[values.count / 2] }
    let result: [String: Any] = [
      "scenario": "cost", "engine": screen.engine.rawValue, "screen": String(describing: type(of: screen)),
      "rows": list.rows.count, "prependMs": median(prepends.map(\.list)), "appendMs": median(appends.map(\.list)),
      "prependTotalMs": median(prepends.map(\.total)), "appendTotalMs": median(appends.map(\.total)),
    ]
    let data = try! JSONSerialization.data(withJSONObject: result, options: .sortedKeys)
    print("[SLSCENARIO] \(String(data: data, encoding: .utf8)!)")
    fflush(stdout)
    if UserDefaults.standard.bool(forKey: "SLBenchExit") {
      exit(0)
    }
  }

  /*
   * Remove the second visible row, which slides the next one up, and add a new row after that
   * one, all through reloadData.
   * Logs the new row's alpha and a survivor's slide while the animation runs, then whether it
   * settled and how far the first visible row moved.
   */
  private static func checkAnimation(_ screen: ListScreen, _ kitList: ShadowListKitListView) {
    let list = screen.list!
    kitList.animatesChanges = true
    let first = kitList.visibleRange.location
    let topBefore = kitList.cellForItem(at: first).map { $0.frame.minY - kitList.contentOffset.y } ?? -1
    let survivorBefore = kitList.cellForItem(at: first + 2).map { $0.center.y - kitList.contentOffset.y } ?? -1
    var rows = list.rows
    rows.remove(at: first + 1)
    rows.insert(FeedRow(post: FeedPost(index: 900_000)), at: first + 2)
    list.setRows(rows, change: .update)
    kitList.layoutIfNeeded()
    after(0.06) {
      let inserted = kitList.cellForItem(at: first + 2)
      let survivor = kitList.cellForItem(at: first + 1)
      let during: [String: Any] = [
        "insertedAlpha": Double(inserted?.layer.presentation()?.opacity ?? -1),
        "survivorSlideY": Double(survivor?.layer.presentation()?.affineTransform().ty ?? 0),
        "survivorScreenYBefore": Double(survivorBefore),
        "survivorScreenYAfter": Double(survivor.map { $0.center.y - kitList.contentOffset.y } ?? -1),
      ]
      after(0.7) {
        let settled = kitList.visibleCells.allSatisfy { $0.alpha == 1 && $0.transform.isIdentity }
        let topAfter = kitList.cellForItem(at: first).map { $0.frame.minY - kitList.contentOffset.y } ?? -1
        finish(["scenario": "animate", "during": during, "settled": settled, "firstRowMoved": Double(topAfter - topBefore)])
      }
    }
  }

  private static func checkAccessibility(_ kitList: ShadowListKitListView) {
    let count = kitList.accessibilityElementCount()
    let target = 500
    // A row off screen is a proxy element, a row on screen its cell.
    let element = kitList.accessibilityElement(at: target)
    var result: [String: Any] = ["scenario": "a11y", "elementCount": count]
    result["elementIsCell"] = element is ShadowListKitListCell
    result["elementOnScreen"] = NSLocationInRange(target, kitList.visibleRange)
    result["indexOfElement"] = element.map { kitList.index(ofAccessibilityElement: $0) } ?? -1
    let before = kitList.contentOffset.y
    result["scrollHandled"] = kitList.accessibilityScroll(.up)
    result["pageScrolledBy"] = Double(kitList.contentOffset.y - before)
    finish(result)
  }

  private static func finish(_ result: [String: Any]) {
    let data = try! JSONSerialization.data(withJSONObject: result, options: .sortedKeys)
    print("[SLSCENARIO] \(String(data: data, encoding: .utf8)!)")
    fflush(stdout)
    if UserDefaults.standard.bool(forKey: "SLBenchExit") {
      exit(0)
    }
  }

  private static func after(_ seconds: Double, _ work: @escaping () -> Void) {
    DispatchQueue.main.asyncAfter(deadline: .now() + seconds, execute: work)
  }

  /*
   * Window y of every row view on screen, by row key.
   */
  static func visibleRows(_ scroll: UIScrollView) -> [String: CGFloat] {
    var rows: [String: CGFloat] = [:]
    let visible = scroll.convert(scroll.bounds.inset(by: scroll.adjustedContentInset), to: nil)
    func walk(_ view: UIView, depth: Int) {
      for child in view.subviews where !child.isHidden {
        if let rowView = child as? RowView, let row = rowView.row {
          let frame = rowView.convert(rowView.bounds, to: nil)
          if frame.intersects(visible) {
            rows[row.key] = frame.minY
          }
        } else if depth < 3 {
          walk(child, depth: depth + 1)
        }
      }
    }
    walk(scroll, depth: 0)
    return rows
  }
}
