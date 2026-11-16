import UIKit

/*
 * Chip and ChoiceChip share label measurement, native actions and leading/trailing slots.
 */
final class ShadowListUtilsChipContent: UIView {
  let button = ShadowListUtilsChipButton(type: .custom)
  let clearButton = ShadowListUtilsChipClearButton(type: .custom)
  let theme: ShadowListUtilsTheme
  var label = ""
  var onPress: (() -> Void)?
  var onClear: (() -> Void)?
  var selected = false
  var variant: ShadowListUtilsChipVariant = .tinted
  var size: ShadowListUtilsChipSize = .regular
  var mono = false
  var maxFontSizeMultiplier: CGFloat?
  var labelOverride: String?
  var clearLabelOverride: String?
  var fillOverride: UIColor?
  var textColorOverride: UIColor?
  var borderWidth: CGFloat = 0
  var borderColor: UIColor = .clear
  var leadingView: UIView? { didSet { oldValue?.removeFromSuperview(); if let leadingView { addSubview(leadingView) } } }

  init(theme: ShadowListUtilsTheme) {
    self.theme = theme
    super.init(frame: .zero)
    isAccessibilityElement = false
    button.titleLabel?.numberOfLines = 1; button.titleLabel?.lineBreakMode = .byTruncatingTail
    button.addAction(UIAction { [weak self] _ in self?.onPress?() }, for: .touchUpInside)
    button.onHighlight = { [weak self] highlighted in
      guard let self else { return }
      self.alpha = ShadowListUtilsPressFeedback.alpha(pressed: highlighted && self.onPress != nil)
    }
    clearButton.addAction(UIAction { [weak self] _ in self?.onClear?() }, for: .touchUpInside)
    addSubview(button); addSubview(clearButton)
    refresh()
  }
  @available(*, unavailable)
  required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  func refresh() {
    let compact = size == .compact
    let plain = variant == .plain && !selected
    let style = compact ? theme.typography.footnote : theme.typography.subhead
    let maximum = maxFontSizeMultiplier.flatMap { $0 >= 1 && $0.isFinite ? $0 : nil }
    let scale = min(ShadowListUtilsPlatform.textScale, maximum ?? .greatestFiniteMagnitude)
    let font = mono ? UIFont.monospacedSystemFont(ofSize: style.fontSize * scale, weight: .medium)
      : UIFont.systemFont(ofSize: style.fontSize * scale, weight: .medium)
    let paragraph = NSMutableParagraphStyle()
    paragraph.minimumLineHeight = ShadowListUtilsPlatform.ceilToPixel(style.lineHeight * scale); paragraph.maximumLineHeight = paragraph.minimumLineHeight
    paragraph.alignment = .center; paragraph.lineBreakMode = .byTruncatingTail
    let color = textColorOverride ?? (selected ? theme.colors.onAccent : plain ? theme.colors.secondaryLabel : theme.colors.accent)
    let title = NSAttributedString(string: label, attributes: [.font: font, .kern: (mono ? -0.6 : style.letterSpacing) * scale,
      .paragraphStyle: paragraph, .baselineOffset: ShadowListUtilsTextStyle.labelBaselineOffset(font, lineHeight: paragraph.maximumLineHeight), .foregroundColor: color])
    button.setAttributedTitle(title, for: .normal)
    button.accessibilityLabel = labelOverride ?? label
    button.accessibilityTraits = selected ? [.button, .selected] : .button
    clearButton.setTitle("×", for: .normal)
    clearButton.titleLabel?.font = .systemFont(ofSize: 15 * ShadowListUtilsPlatform.textScale)
    clearButton.setTitleColor(color.withAlphaComponent(0.7), for: .normal)
    clearButton.accessibilityLabel = clearLabelOverride ?? "Clear \(label)"
    clearButton.isHidden = onClear == nil
    accessibilityElements = onClear == nil ? [button] : [button, clearButton]
    backgroundColor = fillOverride ?? (selected ? theme.colors.accent : plain ? theme.colors.fill : theme.colors.accentSoft)
    layer.cornerRadius = compact ? 14 : 17; layer.borderWidth = borderWidth; layer.borderColor = borderColor.cgColor
    alpha = ShadowListUtilsPressFeedback.alpha(pressed: button.isHighlighted && onPress != nil)
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }

  private var padding: CGFloat { size == .compact ? 11 : 14 }
  private var gap: CGFloat { size == .compact ? 6 : 4 }
  private var leadingWidth: CGFloat { leadingView == nil ? 0 : 14 + gap }
  private var clearSize: CGSize { onClear == nil ? .zero : (clearButton.titleLabel?.sizeThatFits(CGSize(width: 240, height: 1000)) ?? .zero) }
  override var intrinsicContentSize: CGSize {
    let title = button.titleLabel?.sizeThatFits(CGSize(width: 240, height: 1000)) ?? .zero
    let trailing = onClear == nil ? 0 : clearSize.width + gap
    return CGSize(width: min(240, title.width + 2 * (padding + borderWidth) + leadingWidth + trailing), height: size == .compact ? 28 : 34)
  }
  override func sizeThatFits(_ size: CGSize) -> CGSize { intrinsicContentSize }
  override func layoutSubviews() {
    super.layoutSubviews()
    let direction = ShadowListUtilsDirection(width: bounds.width)
    let trailing = onClear == nil ? 0 : clearSize.width + gap
    button.frame = bounds
    let leadingInset = padding + borderWidth + leadingWidth
    let trailingInset = padding + borderWidth + trailing
    button.contentEdgeInsets = UIEdgeInsets(top: 0, left: direction.isRightToLeft ? trailingInset : leadingInset,
      bottom: 0, right: direction.isRightToLeft ? leadingInset : trailingInset)
    if let leadingView { leadingView.frame = direction.place(CGRect(x: padding + borderWidth, y: (bounds.height - 14) / 2, width: 14, height: 14)) }
    let clear = clearSize
    clearButton.frame = direction.place(CGRect(x: bounds.width - padding - borderWidth - clear.width,
      y: (bounds.height - clear.height) / 2, width: clear.width, height: clear.height))
  }
}
