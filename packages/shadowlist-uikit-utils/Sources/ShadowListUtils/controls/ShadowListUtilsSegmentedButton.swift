import UIKit

/* A native action view keeps repeated selected taps controlled instead of changing a store. */
final class ShadowListUtilsSegmentedButton: UIControl {
  var option: ShadowListUtilsSegmentedOption
  var onPress: (() -> Void)?
  var compact = false
  let label = UILabel()
  let divider = UIView()
  let theme: ShadowListUtilsTheme
  override var isHighlighted: Bool { didSet { alpha = ShadowListUtilsPressFeedback.alpha(pressed: isHighlighted && !isSelected) } }

  init(option: ShadowListUtilsSegmentedOption, theme: ShadowListUtilsTheme) {
    self.option = option; self.theme = theme
    super.init(frame: .zero)
    label.numberOfLines = 1; label.textAlignment = .center; label.lineBreakMode = .byTruncatingTail
    label.isAccessibilityElement = false; divider.isAccessibilityElement = false
    divider.backgroundColor = theme.colors.separator; divider.isUserInteractionEnabled = false
    addSubview(label); addSubview(divider)
    isAccessibilityElement = true
    addAction(UIAction { [weak self] _ in self?.onPress?() }, for: .touchUpInside)
  }
  @available(*, unavailable)
  required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  func refresh() {
    let scale = ShadowListUtilsPlatform.textScale
    label.attributedText = NSAttributedString(string: option.label, attributes: [
      .font: UIFont.systemFont(ofSize: 13 * scale, weight: isSelected ? .semibold : .regular),
      .kern: -0.1 * scale, .foregroundColor: compact && isSelected ? theme.colors.accent : theme.colors.label,
    ])
    accessibilityLabel = option.label
    // RN's tab role maps to no additional UIKit trait; selected is a separate state.
    accessibilityTraits = isSelected ? .selected : []
    backgroundColor = isSelected ? theme.colors.background : .clear
    layer.cornerRadius = compact ? 6 : 7
    layer.shadowColor = UIColor.black.cgColor; layer.shadowOpacity = isSelected ? 0.1 : 0
    layer.shadowRadius = 2; layer.shadowOffset = CGSize(width: 0, height: 1)
    alpha = ShadowListUtilsPressFeedback.alpha(pressed: isHighlighted && !isSelected)
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }
  override var intrinsicContentSize: CGSize {
    let size = label.sizeThatFits(CGSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude))
    return CGSize(width: size.width + (compact ? 20 : 0), height: compact ? 30 : 32)
  }
  override func layoutSubviews() {
    super.layoutSubviews()
    let inset: CGFloat = compact ? 10 : 0
    let size = label.sizeThatFits(CGSize(width: max(0, bounds.width - inset * 2), height: CGFloat.greatestFiniteMagnitude))
    label.frame = CGRect(x: inset, y: (bounds.height - size.height) / 2, width: max(0, bounds.width - inset * 2), height: size.height)
    let x: CGFloat = effectiveUserInterfaceLayoutDirection == .rightToLeft ? bounds.width - ShadowListUtilsTheme.hairline : 0
    divider.frame = CGRect(x: x, y: 7, width: ShadowListUtilsTheme.hairline, height: max(0, bounds.height - 14))
  }
  override func accessibilityActivate() -> Bool {
    guard isEnabled else { return false }
    sendActions(for: .touchUpInside); return true
  }
}
