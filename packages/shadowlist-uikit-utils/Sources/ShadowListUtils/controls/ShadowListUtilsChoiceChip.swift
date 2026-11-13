import UIKit

public final class ShadowListUtilsChoiceChip: UIView {
  public var label: String { didSet { refresh() } }
  public var tint: UIColor { didSet { refresh() } }
  public var selected: Bool { didSet { refresh() } }
  public var onPress: (() -> Void)? { didSet { refresh() } }
  public let theme: ShadowListUtilsTheme
  private let content: ShadowListUtilsChipContent
  private let check: ShadowListUtilsIconView

  public init(label: String, tint: UIColor, selected: Bool, theme: ShadowListUtilsTheme = .system) {
    self.label = label; self.tint = tint; self.selected = selected; self.theme = theme
    content = ShadowListUtilsChipContent(theme: theme); check = ShadowListUtilsIconView(kind: .check)
    check.color = theme.colors.label
    super.init(frame: .zero)
    isAccessibilityElement = false; addSubview(content); refresh()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize { content.intrinsicContentSize }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { intrinsicContentSize }
  public override func layoutSubviews() { super.layoutSubviews(); content.frame = bounds }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window); refresh()
    }
  }
  private func refresh() {
    content.label = label; content.onPress = onPress; content.selected = selected
    content.fillOverride = tint; content.textColorOverride = theme.colors.label
    content.borderWidth = 2; content.borderColor = selected ? theme.colors.label : .clear
    content.leadingView = selected ? check : nil
    content.refresh(); invalidateIntrinsicContentSize(); setNeedsLayout()
    superview?.invalidateIntrinsicContentSize(); superview?.setNeedsLayout()
  }
}
