import UIKit

/* Grouped inset/card styling without automatic separators; rows choose their own separation. */
public final class ShadowListUtilsGroupedCard: UIView {
  public var children: [UIView] { didSet { stack.replaceArrangedSubviews(children); invalidateIntrinsicContentSize() } }
  public let theme: ShadowListUtilsTheme
  private let stack = UIStackView()
  public init(children: [UIView] = [], theme: ShadowListUtilsTheme = .system) {
    self.children = children; self.theme = theme
    super.init(frame: .zero)
    stack.axis = .vertical; stack.backgroundColor = theme.colors.groupedCell
    stack.layer.cornerRadius = theme.grouped.radius; stack.clipsToBounds = true
    stack.translatesAutoresizingMaskIntoConstraints = false; addSubview(stack)
    NSLayoutConstraint.activate([stack.topAnchor.constraint(equalTo: topAnchor), stack.bottomAnchor.constraint(equalTo: bottomAnchor), stack.leadingAnchor.constraint(equalTo: leadingAnchor, constant: theme.grouped.inset), stack.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -theme.grouped.inset)])
    stack.replaceArrangedSubviews(children)
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { systemLayoutSizeFitting(CGSize(width: size.width, height: 0), withHorizontalFittingPriority: .required, verticalFittingPriority: .fittingSizeLevel) }
}
