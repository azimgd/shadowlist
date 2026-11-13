import UIKit

public final class ShadowListUtilsGroupedCaption: UIView {
  public var title: String { didSet { applyTitle() } }
  public let theme: ShadowListUtilsTheme
  private let label = UILabel()
  public init(title: String, theme: ShadowListUtilsTheme = .system) {
    self.title = title; self.theme = theme
    super.init(frame: .zero)
    label.font = .systemFont(ofSize: theme.typography.footnote.fontSize * ShadowListUtilsPlatform.textScale)
    label.textColor = theme.colors.secondaryLabel; label.numberOfLines = 0
    label.accessibilityTraits = .header
    label.translatesAutoresizingMaskIntoConstraints = false; addSubview(label)
    let inset = theme.grouped.inset + theme.grouped.rowInset
    NSLayoutConstraint.activate([label.topAnchor.constraint(equalTo: topAnchor), label.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -max(0, theme.spacing.sm - 2)), label.leadingAnchor.constraint(equalTo: leadingAnchor, constant: inset), label.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -inset)])
    applyTitle()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { systemLayoutSizeFitting(CGSize(width: size.width, height: 0), withHorizontalFittingPriority: .required, verticalFittingPriority: .fittingSizeLevel) }
  private func applyTitle() { label.text = theme.grouped.captionCase == .upper ? title.uppercased() : title; invalidateIntrinsicContentSize() }
}
