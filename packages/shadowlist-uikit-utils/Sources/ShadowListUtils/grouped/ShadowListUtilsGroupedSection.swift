import UIKit

public final class ShadowListUtilsGroupedSection: UIView {
  public var title: String? { didSet { updateTitle() } }
  public var children: [UIView] { didSet { card.children = children; invalidateIntrinsicContentSize() } }
  public let theme: ShadowListUtilsTheme
  private let stack = UIStackView()
  private let caption: ShadowListUtilsGroupedCaption
  private let card: ShadowListUtilsGroupedCard
  public init(title: String? = nil, children: [UIView] = [], theme: ShadowListUtilsTheme = .system) {
    self.title = title; self.children = children; self.theme = theme
    caption = ShadowListUtilsGroupedCaption(title: title ?? "", theme: theme)
    card = ShadowListUtilsGroupedCard(children: children, theme: theme)
    super.init(frame: .zero)
    stack.axis = .vertical; stack.translatesAutoresizingMaskIntoConstraints = false; addSubview(stack)
    NSLayoutConstraint.activate([stack.topAnchor.constraint(equalTo: topAnchor), stack.leadingAnchor.constraint(equalTo: leadingAnchor), stack.trailingAnchor.constraint(equalTo: trailingAnchor), stack.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -theme.spacing.xl)])
    stack.addArrangedSubview(caption); stack.addArrangedSubview(card); updateTitle()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { systemLayoutSizeFitting(CGSize(width: size.width, height: 0), withHorizontalFittingPriority: .required, verticalFittingPriority: .fittingSizeLevel) }
  private func updateTitle() { caption.title = title ?? ""; caption.isHidden = title?.isEmpty != false; invalidateIntrinsicContentSize() }
}
