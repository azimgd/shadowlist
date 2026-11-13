import ShadowListUtilsCore
import UIKit

/* Native measurements feed the same wrap algorithm used by the Android adapter. */
public final class ShadowListUtilsChipRow: UIView {
  public var children: [UIView] { didSet { rebuild() } }
  public let theme: ShadowListUtilsTheme
  private var layoutWidth: CGFloat = -1

  public init(children: [UIView] = [], theme: ShadowListUtilsTheme = .system) {
    self.children = children; self.theme = theme
    super.init(frame: .zero)
    rebuild()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize {
    CGSize(width: UIView.noIntrinsicMetric, height: sizeThatFits(CGSize(width: bounds.width, height: .greatestFiniteMagnitude)).height)
  }
  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: size.width, height: measured(width: size.width).height)
  }
  public override func layoutSubviews() {
    super.layoutSubviews()
    if layoutWidth != bounds.width { layoutWidth = bounds.width; invalidateIntrinsicContentSize() }
    let layout = measured(width: bounds.width)
    let direction = ShadowListUtilsDirection(width: bounds.width)
    for (child, frame) in zip(children, layout.frames) { child.frame = direction.place(frame.cgRectValue) }
  }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }
  private func measured(width: CGFloat) -> ShadowListUtilsWrapLayout {
    let sizes = children.map { NSValue(cgSize: $0.sizeThatFits(CGSize(width: width, height: .greatestFiniteMagnitude))) }
    return ShadowListUtilsWrapLayout(width: width, sizes: sizes, gap: theme.spacing.sm)
  }
  private func rebuild() {
    for view in subviews where !children.contains(where: { $0 === view }) { view.removeFromSuperview() }
    for child in children { addSubview(child) }
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }
}
