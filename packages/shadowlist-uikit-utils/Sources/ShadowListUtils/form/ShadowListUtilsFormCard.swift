import UIKit

/* A grouped form card inserts one inset hairline between each pair of native children. */
public final class ShadowListUtilsFormCard: UIView {
  public var children: [UIView] { didSet { if !children.elementsEqual(oldValue, by: { $0 === $1 }) { rebuild() } } }
  public let theme: ShadowListUtilsTheme
  private let stack = UIStackView()
  private var separators: [UIView] = []
  public init(children: [UIView] = [], theme: ShadowListUtilsTheme = .system) {
    self.children = children; self.theme = theme
    super.init(frame: .zero)
    backgroundColor = theme.colors.groupedCell; layer.cornerRadius = theme.grouped.radius; clipsToBounds = true
    stack.axis = .vertical; stack.translatesAutoresizingMaskIntoConstraints = false; addSubview(stack)
    NSLayoutConstraint.activate([stack.topAnchor.constraint(equalTo: topAnchor), stack.bottomAnchor.constraint(equalTo: bottomAnchor), stack.leadingAnchor.constraint(equalTo: leadingAnchor), stack.trailingAnchor.constraint(equalTo: trailingAnchor)])
    rebuild()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    systemLayoutSizeFitting(CGSize(width: size.width, height: 0), withHorizontalFittingPriority: .required, verticalFittingPriority: .fittingSizeLevel)
  }
  private func rebuild() {
    while separators.count < max(0, children.count - 1) {
      let container = UIView(); let line = UIView()
      line.backgroundColor = theme.colors.separator; line.translatesAutoresizingMaskIntoConstraints = false; container.addSubview(line)
      NSLayoutConstraint.activate([container.heightAnchor.constraint(equalToConstant: ShadowListUtilsTheme.hairline), line.topAnchor.constraint(equalTo: container.topAnchor), line.bottomAnchor.constraint(equalTo: container.bottomAnchor), line.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: theme.grouped.rowInset), line.trailingAnchor.constraint(equalTo: container.trailingAnchor)])
      separators.append(container)
    }
    var arranged: [UIView] = []
    for (index, child) in children.enumerated() { if index > 0 { arranged.append(separators[index - 1]) }; arranged.append(child) }
    stack.replaceArrangedSubviews(arranged)
    invalidateIntrinsicContentSize()
  }
}
