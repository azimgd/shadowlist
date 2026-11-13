import UIKit

/* Standalone total-vote footer, also used by PollList's owned default footer. */
public final class ShadowListUtilsPollFooter: UIView {
  public var totalVotes: Double { didSet { rebuild() } }
  public var labels = ShadowListUtilsPollLabels() { didSet { rebuild() } }
  public var theme: ShadowListUtilsTheme { didSet { rebuild() } }
  private var footer: ShadowListUtilsListFooter?
  private let separator = UIView()
  public init(totalVotes: Double, theme: ShadowListUtilsTheme = .system) {
    self.totalVotes = totalVotes; self.theme = theme
    super.init(frame: .zero)
    addSubview(separator); rebuild()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: sizeThatFits(CGSize(width: max(1, bounds.width), height: CGFloat.greatestFiniteMagnitude)).height) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { footer?.sizeThatFits(size) ?? .zero }
  public override func layoutSubviews() {
    super.layoutSubviews(); footer?.frame = bounds
    separator.frame = CGRect(x: 0, y: 0, width: bounds.width, height: 1 / UIScreen.main.scale)
    bringSubviewToFront(separator)
  }
  private func rebuild() {
    footer?.removeFromSuperview()
    var footerTheme = theme
    footerTheme.spacing.xl = theme.spacing.lg
    let view = ShadowListUtilsListFooter(text: labels.totalVotes(totalVotes), theme: footerTheme)
    footer = view; addSubview(view); separator.backgroundColor = theme.colors.separator
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }
}
