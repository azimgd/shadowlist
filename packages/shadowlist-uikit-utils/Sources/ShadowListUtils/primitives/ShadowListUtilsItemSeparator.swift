import UIKit

/*
 * A hairline between two rows that starts after the leading inset, by default where a row's
 * text starts. The inset shows the theme's background.
 */
public final class ShadowListUtilsItemSeparator: UIView {
  public var theme: ShadowListUtilsTheme {
    didSet { applyTheme() }
  }

  public var inset: CGFloat {
    didSet { setNeedsLayout() }
  }

  private let line = UIView()

  public init(theme: ShadowListUtilsTheme) {
    self.theme = theme
    inset = theme.rowInset
    super.init(frame: CGRect(x: 0, y: 0, width: 0, height: ShadowListUtilsTheme.hairline))
    isUserInteractionEnabled = false
    isAccessibilityElement = false
    addSubview(line)
    applyTheme()
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: size.width, height: ShadowListUtilsTheme.hairline)
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    let frame = CGRect(x: inset, y: 0, width: max(0, bounds.width - inset), height: bounds.height)
    line.frame = ShadowListUtilsDirection(width: bounds.width).place(frame)
  }

  private func applyTheme() {
    backgroundColor = theme.colors.background
    line.backgroundColor = theme.colors.separator
  }
}
