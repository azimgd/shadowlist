import UIKit

/* Optional press and clear actions are independent; selection remains application-owned. */
public final class ShadowListUtilsChip: UIView {
  public var label: String { didSet { refresh() } }
  public var onPress: (() -> Void)? { didSet { refresh() } }
  public var onClear: (() -> Void)? { didSet { refresh() } }
  public var selected = false { didSet { refresh() } }
  public var variant: ShadowListUtilsChipVariant = .tinted { didSet { refresh() } }
  public var size: ShadowListUtilsChipSize = .regular { didSet { refresh() } }
  public var mono = false { didSet { refresh() } }
  public var maxFontSizeMultiplier: CGFloat? { didSet { refresh() } }
  public var clearAccessibilityLabel: String? { didSet { refresh() } }
  public override var accessibilityLabel: String? { didSet { refresh() } }
  public let theme: ShadowListUtilsTheme
  private let content: ShadowListUtilsChipContent

  public init(label: String, theme: ShadowListUtilsTheme = .system) {
    self.label = label; self.theme = theme; content = ShadowListUtilsChipContent(theme: theme)
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
    content.label = label; content.onPress = onPress; content.onClear = onClear
    content.selected = selected; content.variant = variant; content.size = size; content.mono = mono
    content.maxFontSizeMultiplier = maxFontSizeMultiplier
    content.labelOverride = accessibilityLabel; content.clearLabelOverride = clearAccessibilityLabel
    content.refresh(); invalidateIntrinsicContentSize(); setNeedsLayout()
    superview?.invalidateIntrinsicContentSize(); superview?.setNeedsLayout()
  }
}
