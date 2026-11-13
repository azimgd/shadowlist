import ShadowListKit
import UIKit

/* The existing native action surface around caller-owned grouped content. */
public final class ShadowListUtilsGroupedActions: UIView {
  public var children: [UIView] { didSet { stack.replaceArrangedSubviews(children); invalidateIntrinsicContentSize(); setNeedsLayout() } }
  public var theme: ShadowListUtilsTheme { didSet { updateActions() } }
  public var leadingSwipeActions: ShadowListKitSwipeActionsConfiguration? { didSet { updateActions() } }
  public var trailingSwipeActions: ShadowListKitSwipeActionsConfiguration? { didSet { updateActions() } }
  public var contextMenu: UIMenu? { didSet { actions.contextMenu = contextMenu } }
  public var onContextMenuWillShow: (() -> Void)? { didSet { actions.onContextMenuWillShow = onContextMenuWillShow } }

  private let actions = ShadowListKitItemActionsView()
  private let stack = UIStackView()
  private var layoutWidth: CGFloat = -1

  public init(children: [UIView], theme: ShadowListUtilsTheme = .system) {
    self.children = children; self.theme = theme
    super.init(frame: .zero)
    stack.axis = .vertical
    stack.translatesAutoresizingMaskIntoConstraints = false
    actions.contentView.addSubview(stack)
    addSubview(actions)
    NSLayoutConstraint.activate([
      stack.topAnchor.constraint(equalTo: actions.contentView.topAnchor),
      stack.bottomAnchor.constraint(equalTo: actions.contentView.bottomAnchor),
      stack.leadingAnchor.constraint(equalTo: actions.contentView.leadingAnchor),
      stack.trailingAnchor.constraint(equalTo: actions.contentView.trailingAnchor),
    ])
    stack.replaceArrangedSubviews(children)
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  public func closeSwipeActions(animated: Bool = true) { actions.closeSwipeActions(animated: animated) }

  var swipeAccessibilityActions: [UIAccessibilityCustomAction] { actions.accessibilityCustomActions ?? [] }

  public override var intrinsicContentSize: CGSize {
    CGSize(width: UIView.noIntrinsicMetric, height: sizeThatFits(CGSize(width: bounds.width, height: CGFloat.greatestFiniteMagnitude)).height)
  }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { actions.sizeThatFits(size) }
  public override func layoutSubviews() {
    super.layoutSubviews()
    actions.frame = bounds
    if layoutWidth != bounds.width { layoutWidth = bounds.width; invalidateIntrinsicContentSize() }
  }
  private func updateActions() {
    actions.leadingSwipeActions = leadingSwipeActions?.applying(defaultDestructiveColor: theme.colors.red)
    actions.trailingSwipeActions = trailingSwipeActions?.applying(defaultDestructiveColor: theme.colors.red)
  }
}
