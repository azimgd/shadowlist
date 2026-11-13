import ShadowListKit
import UIKit

/*
 * A large title with an optional subtitle, for a list's headerView. The list sizes it through
 * sizeThatFits.
 */
public final class ShadowListUtilsListHeader: UIView {
  public var theme: ShadowListUtilsTheme {
    didSet { applyTheme() }
  }

  public let title: String
  public let subtitle: String?
  private let titleView = ShadowListKitTextView(frame: .zero)
  private let subtitleView = ShadowListKitTextView(frame: .zero)
  private var laidOutWidth: CGFloat = -1
  private var contentHeight: CGFloat = 0

  public init(title: String, subtitle: String? = nil, theme: ShadowListUtilsTheme) {
    self.title = title
    self.subtitle = subtitle?.isEmpty == false ? subtitle : nil
    self.theme = theme
    super.init(frame: .zero)
    titleView.accessibilityTraits = .header
    titleView.isAccessibilityElement = true
    titleView.accessibilityLabel = title
    subtitleView.isAccessibilityElement = self.subtitle != nil
    subtitleView.accessibilityLabel = self.subtitle
    addSubview(titleView)
    addSubview(subtitleView)
    applyTheme()
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    layOut(width: size.width)
    return CGSize(width: size.width, height: contentHeight)
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    layOut(width: bounds.width)
  }

  private func applyTheme() {
    backgroundColor = theme.colors.background
    laidOutWidth = -1
    setNeedsLayout()
  }

  /*
   * Typeset both lines for a width and place them, once per width.
   */
  private func layOut(width: CGFloat) {
    guard width != laidOutWidth, width > 0 else { return }
    laidOutWidth = width
    let spacing = theme.spacing
    let textWidth = width - 2 * spacing.lg
    let direction = ShadowListUtilsDirection(width: width)
    let titleLayout = theme.typography.largeTitle.layout(title, width: textWidth)
    titleView.show(titleLayout, theme.colors.label)
    titleView.frame = direction.place(CGRect(CGPoint(x: spacing.lg, y: spacing.xs), titleLayout))
    var bottom = titleView.frame.maxY
    if let subtitle {
      let subtitleLayout = theme.typography.subhead.layout(subtitle, width: textWidth)
      subtitleView.show(subtitleLayout, theme.colors.secondaryLabel)
      subtitleView.frame = direction.place(CGRect(CGPoint(x: spacing.lg, y: bottom + spacing.xxs), subtitleLayout))
      bottom = subtitleView.frame.maxY
    }
    subtitleView.isHidden = subtitle == nil
    contentHeight = bottom + spacing.md
  }
}
