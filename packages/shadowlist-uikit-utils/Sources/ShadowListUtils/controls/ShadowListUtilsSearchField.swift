import UIKit

public final class ShadowListUtilsSearchField: UIView {
  public var value: String { didSet { editing.value = value } }
  public var onChange: ((String) -> Void)?
  public var icon: UIView? { didSet { updateIcon() } }
  public var labels = ShadowListUtilsSearchFieldLabels() { didSet { input.placeholder = labels.placeholder; input.accessibilityLabel = labels.placeholder } }
  public let theme: ShadowListUtilsTheme
  let input = ShadowListUtilsTextInputField()
  private let defaultIcon = UIImageView(image: UIImage(systemName: "magnifyingglass"))
  private weak var shownIcon: UIView?
  private lazy var editing = ShadowListUtilsTextInput(input: .textField(input), defaultValue: value)

  public init(value: String, theme: ShadowListUtilsTheme = .system) {
    self.value = value; self.theme = theme
    super.init(frame: .zero)
    backgroundColor = theme.colors.elevated2; layer.cornerRadius = 10
    input.font = .systemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale)
    input.defaultTextAttributes[.kern] = theme.typography.body.letterSpacing * ShadowListUtilsPlatform.textScale
    input.textColor = theme.colors.label; input.placeholder = labels.placeholder
    input.accessibilityLabel = labels.placeholder
    input.autocorrectionType = .no; input.autocapitalizationType = .none
    input.clearButtonMode = .always
    defaultIcon.tintColor = theme.colors.secondaryLabel; defaultIcon.contentMode = .scaleAspectFit
    defaultIcon.isUserInteractionEnabled = false; defaultIcon.isAccessibilityElement = false
    addSubview(input)
    editing.value = value; editing.onChangeText = { [weak self] in self?.onChange?($0) }
    updateIcon()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: 36) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: 36) }
  public override func layoutSubviews() {
    super.layoutSubviews()
    let direction = ShadowListUtilsDirection(width: bounds.width)
    let measured = shownIcon?.systemLayoutSizeFitting(UIView.layoutFittingCompressedSize) ?? .zero
    let iconWidth = icon == nil ? 16 : max(0, measured.width > 0 ? measured.width : shownIcon?.bounds.width ?? 0)
    let iconHeight = icon == nil ? 16 : max(0, measured.height > 0 ? measured.height : shownIcon?.bounds.height ?? 0)
    shownIcon?.frame = direction.place(CGRect(x: 10, y: (bounds.height - iconHeight) / 2, width: iconWidth, height: iconHeight))
    let leading = 10 + iconWidth + 6
    input.frame = direction.place(CGRect(x: leading, y: 0, width: max(0, bounds.width - leading - 10), height: bounds.height))
  }
  public override var canBecomeFirstResponder: Bool { input.canBecomeFirstResponder }
  public override func becomeFirstResponder() -> Bool { input.becomeFirstResponder() }
  public override func resignFirstResponder() -> Bool { input.resignFirstResponder() }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window)
      input.font = .systemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale)
      input.defaultTextAttributes[.kern] = theme.typography.body.letterSpacing * ShadowListUtilsPlatform.textScale
    }
  }
  private func updateIcon() {
    let next = icon ?? defaultIcon
    if shownIcon !== next { shownIcon?.removeFromSuperview(); shownIcon = next; addSubview(next) }
    setNeedsLayout()
  }
}
