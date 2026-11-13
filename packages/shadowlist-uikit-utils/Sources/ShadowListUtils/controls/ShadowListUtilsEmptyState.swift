import ShadowListKit
import UIKit

/*
 * What a screen says before it has anything to show: an optional icon, a title and an optional
 * message in a centered column. It is read as one accessibility element.
 */
public final class ShadowListUtilsEmptyState: UIView {
  public var theme: ShadowListUtilsTheme {
    didSet { applyTheme() }
  }

  public let title: String
  public let message: String?
  public let icon: UIView?
  private let titleView = ShadowListKitTextView(frame: .zero)
  private let messageView = ShadowListKitTextView(frame: .zero)
  private var laidOutWidth: CGFloat = -1
  private var contentHeight: CGFloat = 0

  public init(title: String, message: String? = nil, icon: UIView? = nil, theme: ShadowListUtilsTheme) {
    self.title = title
    self.message = message
    self.icon = icon
    self.theme = theme
    super.init(frame: .zero)
    isAccessibilityElement = true
    accessibilityLabel = message.map { "\(title). \($0)" } ?? title
    if let icon {
      addSubview(icon)
    }
    addSubview(titleView)
    addSubview(messageView)
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
    laidOutWidth = -1
    setNeedsLayout()
  }

  /*
   * Stack the icon, the title and the message from the top, each centered, once per width.
   */
  private func layOut(width: CGFloat) {
    guard width != laidOutWidth, width > 0 else { return }
    laidOutWidth = width
    let gap = theme.spacing.xs
    let textWidth = width - 2 * theme.grouped.inset
    var y = Self.paddingVertical
    if let icon {
      let size = icon.bounds.size == .zero ? icon.sizeThatFits(CGSize(width: textWidth, height: .greatestFiniteMagnitude)) : icon.bounds.size
      icon.frame = CGRect(x: ShadowListUtilsPlatform.roundToPixel((width - size.width) / 2), y: y, width: size.width, height: size.height)
      y = icon.frame.maxY + gap
    }
    // The title is the body style at medium weight, which the public weights do not name.
    let titleLayout = theme.typography.body.layout(title, width: textWidth, maxLines: 0, scaled: true, alignment: .center, weight: .medium)
    titleView.show(titleLayout, theme.colors.label)
    // Centered lines are placed inside the width they were typeset for.
    titleView.frame = CGRect(x: theme.grouped.inset, y: y + Self.titleMarginTop, width: textWidth, height: titleLayout.size.height)
    y = titleView.frame.maxY
    messageView.isHidden = message == nil
    if let message {
      let messageLayout = theme.typography.footnote.layout(message, width: textWidth, maxLines: 0, scaled: true, alignment: .center)
      messageView.show(messageLayout, theme.colors.secondaryLabel)
      messageView.frame = CGRect(x: theme.grouped.inset, y: y + gap, width: textWidth, height: messageLayout.size.height)
      y = messageView.frame.maxY
    }
    contentHeight = y + Self.paddingVertical
  }

  private static let paddingVertical: CGFloat = 28
  private static let titleMarginTop: CGFloat = 6
}
