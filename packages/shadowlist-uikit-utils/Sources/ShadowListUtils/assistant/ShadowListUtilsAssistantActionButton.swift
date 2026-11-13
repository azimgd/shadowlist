import UIKit

/*
 * An assistant action. Native images take the place of the React view's icon child.
 */
public final class ShadowListUtilsAssistantActionButton: UIButton {
  public var label: String { didSet { apply() } }
  public var icon: UIImage? { didSet { apply() } }
  public var showLabel: Bool { didSet { apply() } }
  public var onPress: (() -> Void)?
  public let theme: ShadowListUtilsTheme
  public override var isEnabled: Bool { didSet { apply() } }
  public override var isSelected: Bool { didSet { apply() } }

  public init(
    label: String, icon: UIImage? = nil, showLabel: Bool = false, disabled: Bool = false,
    selected: Bool = false, theme: ShadowListUtilsTheme = .system, onPress: (() -> Void)? = nil
  ) {
    self.label = label
    self.icon = icon
    self.showLabel = showLabel
    self.theme = theme
    self.onPress = onPress
    super.init(frame: .zero)
    isEnabled = !disabled
    isSelected = selected
    addTarget(self, action: #selector(press), for: .touchUpInside)
    apply()
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  private func apply() {
    accessibilityLabel = label
    accessibilityTraits = isSelected ? [.button, .selected] : .button
    var configuration = UIButton.Configuration.plain()
    configuration.title = showLabel ? label : nil
    configuration.image = icon
    configuration.imagePadding = theme.spacing.xs
    configuration.contentInsets = NSDirectionalEdgeInsets(top: 0, leading: showLabel ? 10 : 0, bottom: 0, trailing: showLabel ? 10 : 0)
    configuration.background.backgroundColor = showLabel ? theme.colors.elevated : .clear
    configuration.background.cornerRadius = theme.radius.sm
    configuration.baseForegroundColor = theme.colors.label
    configuration.titleTextAttributesTransformer = UIConfigurationTextAttributesTransformer { [theme] attributes in
      var attributes = attributes
      attributes.font = UIFont.systemFont(ofSize: theme.typography.footnote.fontSize * ShadowListUtilsPlatform.textScale, weight: .semibold)
      return attributes
    }
    self.configuration = configuration
    alpha = isEnabled ? 1 : 0.35
    invalidateIntrinsicContentSize()
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    let fitted = super.sizeThatFits(size)
    return CGSize(width: max(32, fitted.width), height: 32)
  }

  public override var intrinsicContentSize: CGSize { CGSize(width: max(32, super.intrinsicContentSize.width), height: 32) }

  @objc func press() { if isEnabled { onPress?() } }
}
