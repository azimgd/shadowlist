import ShadowListUtils
import UIKit

/*
 * The same release backlog as the React Native example, with app-owned move state.
 */
final class BoardScreen: UIViewController {
  private var columns = BoardFixtures.columns
  private var added = 0
  private let hint = ShadowListUtilsListFooter(text: "Touch and hold a card, then drag it up, down or into another column.", theme: AppTheme.theme)
  private var hintHeight: NSLayoutConstraint!
  private lazy var board = ShadowListUtilsBoardList<BoardExampleCard>(theme: AppTheme.theme, keyExtractor: { $0.key }, rowForItem: {
    ShadowListUtilsBoardCard(key: $0.key, title: $0.title, label: $0.label, detail: $0.due,
      color: $0.color, theme: AppTheme.theme)
  })

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Release Board"
    view.backgroundColor = AppTheme.colors.background
    hint.translatesAutoresizingMaskIntoConstraints = false
    board.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(hint)
    view.addSubview(board)
    hintHeight = hint.heightAnchor.constraint(equalToConstant: 76)
    NSLayoutConstraint.activate([
      hintHeight,
      hint.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
      hint.leadingAnchor.constraint(equalTo: view.leadingAnchor), hint.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      board.topAnchor.constraint(equalTo: hint.bottomAnchor), board.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      board.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      board.bottomAnchor.constraint(equalTo: view.bottomAnchor),
    ])
    board.renderColumnHeader = { column in BoardColumnHeading(title: column.title, count: column.items.count) }
    board.renderColumnFooter = { [weak self] column in
      let button = ShadowListUtilsDashedButton(label: "Add card", theme: AppTheme.theme)
      button.onPress = { [weak self] in
        guard let self, let index = columns.firstIndex(where: { $0.key == column.key }) else { return }
        added += 1
        columns[index].items.append(BoardExampleCard(key: "added-\(added)", title: "New travel task", label: "Design", due: "Today"))
        board.setColumns(columns, animated: true)
      }
      return BoardColumnFooter(button: button)
    }
    board.onMoveItem = { [weak self] move in
      guard let self else { return }
      columns = shadowListUtilsMoveBoardItem(columns, move: move, keyExtractor: { $0.key })
      board.setColumns(columns, animated: true)
      UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    }
    board.columns = columns
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    let height = hint.sizeThatFits(CGSize(width: view.bounds.width, height: .greatestFiniteMagnitude)).height
    if hintHeight.constant != height { hintHeight.constant = height }
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    BoardScenario.start(board)
  }
}
