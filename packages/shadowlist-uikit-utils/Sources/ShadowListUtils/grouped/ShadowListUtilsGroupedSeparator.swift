import UIKit

public final class ShadowListUtilsGroupedSeparator: UIView {
  public var inset: CGFloat? { didSet { setNeedsLayout() } }
  public let theme: ShadowListUtilsTheme
  private let line = UIView()
  public init(inset: CGFloat? = nil, theme: ShadowListUtilsTheme = .system) {
    self.inset = inset; self.theme = theme
    super.init(frame: .zero)
    line.backgroundColor = theme.colors.separator; addSubview(line)
    isAccessibilityElement = false
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: ShadowListUtilsTheme.hairline) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: ShadowListUtilsTheme.hairline) }
  public override func layoutSubviews() {
    super.layoutSubviews()
    let start = max(0, inset ?? theme.grouped.rowInset)
    line.frame = ShadowListUtilsDirection(width: bounds.width).place(CGRect(x: start, y: 0, width: max(0, bounds.width - start), height: ShadowListUtilsTheme.hairline))
  }
}
