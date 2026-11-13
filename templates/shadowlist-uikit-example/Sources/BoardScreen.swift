import ShadowListUtils
import UIKit

/*
 * Generic application cards on a controlled board. The app applies each shared move plan.
 */
final class BoardScreen: UIViewController {
  private struct Card {
    let key: String
    let title: String
    let label: String
  }
  private var columns: [ShadowListUtilsBoardColumn<Card>] = [
    ShadowListUtilsBoardColumn(key: "todo", title: "To do", items: [Card(key: "a", title: "Plan the journey", label: "Travel"), Card(key: "b", title: "Book the window seat", label: "Flight")]),
    ShadowListUtilsBoardColumn(key: "progress", title: "In progress", items: [Card(key: "c", title: "Pack the camera", label: "Equipment")]),
    ShadowListUtilsBoardColumn(key: "done", title: "Done", items: []),
  ]
  private lazy var board = ShadowListUtilsBoardList<Card>(theme: AppTheme.theme, keyExtractor: { $0.key }, rowForItem: {
    ShadowListUtilsBoardCard(key: $0.key, title: $0.title, label: $0.label, color: AppTheme.colors.accent, theme: AppTheme.theme)
  })

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Travel board"
    view.backgroundColor = AppTheme.colors.background
    board.frame = view.bounds
    board.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    view.addSubview(board)
    board.onMoveItem = { [weak self] move in
      guard let self else { return }
      columns = shadowListUtilsMoveBoardItem(columns, move: move, keyExtractor: { $0.key })
      board.setColumns(columns, animated: true)
    }
    board.columns = columns
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    BoardScenario.start(board)
  }
}
