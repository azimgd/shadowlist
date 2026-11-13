import UIKit

/*
 * An activity title, optional subtitle and wrapping action buttons. Set it as headerView.
 */
public final class ShadowListUtilsActivityHeader: UIView {
  public let title: String
  public let subtitle: String?
  public let actions: [ShadowListUtilsActivityHeaderAction]
  public let theme: ShadowListUtilsTheme
  private let header: ShadowListUtilsListHeader
  private var buttons: [UIButton] = []

  public init(
    title: String, subtitle: String? = nil, actions: [ShadowListUtilsActivityHeaderAction] = [],
    theme: ShadowListUtilsTheme = .system
  ) {
    self.title = title
    self.subtitle = subtitle
    self.actions = actions
    self.theme = theme
    header = ShadowListUtilsListHeader(title: title, subtitle: subtitle, theme: theme)
    super.init(frame: .zero)
    backgroundColor = theme.colors.background
    addSubview(header)
    for action in actions {
      var configuration = UIButton.Configuration.plain()
      configuration.title = action.label
      configuration.image = action.icon
      configuration.imagePadding = theme.spacing.xs
      configuration.contentInsets = NSDirectionalEdgeInsets(
        top: theme.spacing.sm, leading: 14, bottom: theme.spacing.sm, trailing: 14)
      configuration.baseForegroundColor = theme.colors.accent
      configuration.background.backgroundColor = theme.colors.accentSoft
      configuration.background.cornerRadius = theme.radius.sm
      configuration.titleTextAttributesTransformer = UIConfigurationTextAttributesTransformer { attributes in
        var attributes = attributes
        attributes.font = UIFont.systemFont(ofSize: theme.typography.footnote.fontSize * ShadowListUtilsPlatform.textScale, weight: .semibold)
        return attributes
      }
      let button = UIButton(configuration: configuration, primaryAction: UIAction { _ in action.onPress() })
      button.accessibilityLabel = action.accessibilityLabel ?? action.label
      buttons.append(button)
      addSubview(button)
    }
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: size.width, height: place(width: size.width))
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    _ = place(width: bounds.width)
  }

  private func place(width: CGFloat) -> CGFloat {
    guard width > 0 else { return 0 }
    let headerHeight = header.sizeThatFits(CGSize(width: width, height: .greatestFiniteMagnitude)).height
    header.frame = CGRect(x: 0, y: 0, width: width, height: headerHeight)
    guard !buttons.isEmpty else { return headerHeight }
    let direction = ShadowListUtilsDirection(width: width)
    let inset = theme.spacing.lg
    var x = inset
    var y = headerHeight - theme.spacing.md + 14
    var lineHeight: CGFloat = 0
    for button in buttons {
      let size = button.sizeThatFits(CGSize(width: max(1, width - 2 * inset), height: .greatestFiniteMagnitude))
      let buttonWidth = min(size.width, max(1, width - 2 * inset))
      if x > inset, x + buttonWidth > width - inset {
        x = inset
        y += lineHeight + theme.spacing.sm
        lineHeight = 0
      }
      button.frame = direction.place(CGRect(x: x, y: y, width: buttonWidth, height: size.height))
      lineHeight = max(lineHeight, size.height)
      x += buttonWidth + theme.spacing.sm
    }
    return y + lineHeight + theme.spacing.md
  }
}
