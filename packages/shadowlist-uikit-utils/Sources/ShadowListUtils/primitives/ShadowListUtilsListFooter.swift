import ShadowListKit
import UIKit

/*
 * A centered line of secondary text, for a list's footerView. The list sizes it through
 * sizeThatFits.
 */
public final class ShadowListUtilsListFooter: UIView {
  public var theme: ShadowListUtilsTheme {
    didSet { applyTheme() }
  }

  public let text: String
  private let textView = ShadowListKitTextView(frame: .zero)
  private var laidOutWidth: CGFloat = -1
  private var contentHeight: CGFloat = 0

  public init(text: String, theme: ShadowListUtilsTheme) {
    self.text = text
    self.theme = theme
    super.init(frame: .zero)
    textView.isAccessibilityElement = true
    textView.accessibilityLabel = text
    addSubview(textView)
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

  private func layOut(width: CGFloat) {
    guard width != laidOutWidth, width > 0 else { return }
    laidOutWidth = width
    let textWidth = width - 2 * theme.spacing.lg
    let layout = theme.typography.footnote.layout(text, width: textWidth, maxLines: 0, scaled: true, alignment: .center)
    textView.show(layout, theme.colors.secondaryLabel)
    // Centered lines are placed inside the width they were typeset for.
    textView.frame = CGRect(x: theme.spacing.lg, y: theme.spacing.xl, width: textWidth, height: layout.size.height)
    contentHeight = layout.size.height + 2 * theme.spacing.xl
  }
}
