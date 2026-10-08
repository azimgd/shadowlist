import ShadowListKit
import UIKit

final class ReorderScreen: ListScreen {
  private var contacts: [Contact] = (0..<80).map { Contact(index: $0) }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Boarding Order"
    list.header = ListFooterView("Touch and hold a traveller, then drag to change the order.")
    list.onMove = { [weak self] from, to in
      guard let self else { return }
      let contact = self.contacts.remove(at: from)
      self.contacts.insert(contact, at: to)
      UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    }
    list.reorderEnabled = true
    list.setRows(contacts.map { ReorderRow(contact: $0) }, change: .reset)
    AutoDrag.run(self, list: list.view) { [weak self] in self?.contacts.map(\.author.name) ?? [] }
  }
}

final class SnapScreen: ListScreen {
  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Destinations"
    list.snapToItem = true
    let height = (UIScreen.main.bounds.height / 4).rounded()
    list.setRows((0..<50).map { SnapRow(index: $0, height: height) }, change: .reset)
  }
}

/*
 * -SLAutoDrag 1 on the reorder screen: pick up the second row, carry it down four rows and drop
 * it, then log the order. Without touches, through the list's testing hooks.
 */
enum AutoDrag {
  static func run(_ screen: ReorderScreen, list: UIScrollView, names: @escaping () -> [String]) {
    guard UserDefaults.standard.string(forKey: "SLAutoDrag") == "1", let list = list as? SLKListView else { return }
    DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
      let before = names().prefix(6)
      let start = list.rectForItem(at: 1)
      var point = CGPoint(x: start.midX, y: start.midY)
      list.slk_beginDrag(at: point)
      var step = 0
      Timer.scheduledTimer(withTimeInterval: 1.0 / 60, repeats: true) { timer in
        step += 1
        point.y += 67 * 4.2 / 30
        list.slk_moveDrag(to: point)
        if step == 30 {
          timer.invalidate()
          list.slk_endDrag()
          DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) {
            print("[SLAUTODRAG] before=\(Array(before)) after=\(Array(names().prefix(6)))")
            fflush(stdout)
          }
        }
      }
    }
  }
}
