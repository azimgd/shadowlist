import UIKit

/* Native equivalent of FormRow's stacked context; only FormInput receives its alignment. */
public final class ShadowListUtilsFormRow: UIView {
  public var label: String { didSet { labelView.text = label; invalidateIntrinsicContentSize() } }
  public var children: [UIView] { didSet { applyLayout() } }
  public var stacked = false { didSet { applyLayout() } }
  public let theme: ShadowListUtilsTheme
  private let row = UIStackView()
  private let control = UIStackView()
  private let labelView = UILabel()
  private let spacer = UIView()
  private var labelWidth: NSLayoutConstraint!

  public init(label: String, children: [UIView], theme: ShadowListUtilsTheme = .system) {
    self.label = label; self.children = children; self.theme = theme
    super.init(frame: .zero)
    labelView.text = label; labelView.numberOfLines = 2; labelView.textColor = theme.colors.label
    labelView.setContentCompressionResistancePriority(.required, for: .horizontal)
    control.axis = .horizontal; control.alignment = .center
    row.isLayoutMarginsRelativeArrangement = true
    row.translatesAutoresizingMaskIntoConstraints = false; addSubview(row)
    row.addArrangedSubview(labelView); row.addArrangedSubview(control)
    labelWidth = labelView.widthAnchor.constraint(lessThanOrEqualTo: row.layoutMarginsGuide.widthAnchor, multiplier: 0.5)
    NSLayoutConstraint.activate([row.topAnchor.constraint(equalTo: topAnchor), row.bottomAnchor.constraint(equalTo: bottomAnchor), row.leadingAnchor.constraint(equalTo: leadingAnchor), row.trailingAnchor.constraint(equalTo: trailingAnchor), heightAnchor.constraint(greaterThanOrEqualToConstant: theme.grouped.rowHeight)])
    spacer.setContentHuggingPriority(.init(1), for: .horizontal)
    applyLayout()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { systemLayoutSizeFitting(CGSize(width: size.width, height: 0), withHorizontalFittingPriority: .required, verticalFittingPriority: .fittingSizeLevel) }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window)
      applyLayout()
    }
  }
  private func applyLayout() {
    let over = stacked || ShadowListUtilsPlatform.textScale >= 1.3
    row.axis = over ? .vertical : .horizontal
    row.alignment = over ? .fill : .center
    row.spacing = over ? theme.spacing.sm : theme.spacing.md
    row.layoutMargins = UIEdgeInsets(top: over ? 10 : 6, left: theme.grouped.rowInset, bottom: over ? 10 : 6, right: theme.grouped.rowInset)
    labelView.font = .systemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale)
    labelWidth.isActive = !over
    for child in children { (child as? ShadowListUtilsFormInput)?.stacked = over }
    let fills = children.contains { $0 is ShadowListUtilsFormInput }
    control.replaceArrangedSubviews((over || fills ? [] : [spacer]) + children)
    invalidateIntrinsicContentSize()
  }
}
