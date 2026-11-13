import ShadowListKit
import UIKit

/* An empty-state overlay; it never contributes rows to the transcript's content size. */
public final class ShadowListUtilsAssistantEmpty: UIView {
  public var suggestions: [ShadowListUtilsAssistantSuggestion] = [] { didSet { rebuildSuggestions() } }
  public var onSelectSuggestion: ((ShadowListUtilsAssistantSuggestion) -> Void)? { didSet { buttons.forEach { $0.isEnabled = onSelectSuggestion != nil } } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { setNeedsLayout(); invalidateIntrinsicContentSize() } }
  public let theme: ShadowListUtilsTheme
  private let mark = UIImageView(image: UIImage(systemName: "sparkles"))
  private let title = ShadowListKitTextView(frame: .zero)
  private let subtitle = ShadowListKitTextView(frame: .zero)
  private var buttons: [UIButton] = []
  public init(theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    super.init(frame: .zero)
    mark.backgroundColor = theme.colors.accent; mark.tintColor = theme.colors.onAccent
    mark.contentMode = .center; mark.layer.cornerRadius = 28
    addSubview(mark); addSubview(title); addSubview(subtitle)
    title.isAccessibilityElement = true; title.accessibilityTraits = .header
    subtitle.isAccessibilityElement = true
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  private func rebuildSuggestions() {
    buttons.forEach { $0.removeFromSuperview() }
    buttons = suggestions.map { suggestion in
      let button = UIButton(type: .system)
      var configuration = UIButton.Configuration.plain()
      configuration.title = suggestion.title; configuration.subtitle = suggestion.prompt
      configuration.titleAlignment = .leading
      configuration.baseForegroundColor = theme.colors.label
      configuration.contentInsets = NSDirectionalEdgeInsets(top: theme.spacing.md, leading: theme.spacing.md, bottom: theme.spacing.md, trailing: theme.spacing.md)
      configuration.titleLineBreakMode = .byWordWrapping
      configuration.subtitleLineBreakMode = .byTruncatingTail
      button.configuration = configuration
      button.backgroundColor = theme.colors.elevated; button.layer.cornerRadius = theme.radius.md
      button.contentHorizontalAlignment = .leading
      button.accessibilityLabel = suggestion.title; button.accessibilityHint = suggestion.prompt
      button.isEnabled = onSelectSuggestion != nil
      button.addAction(UIAction { [weak self] _ in self?.select(suggestion) }, for: .touchUpInside)
      addSubview(button)
      return button
    }
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }
  func select(_ suggestion: ShadowListUtilsAssistantSuggestion) { onSelectSuggestion?(suggestion) }

  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: layout(width: max(1, bounds.width), apply: false)) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: layout(width: size.width, apply: false)) }
  public override func layoutSubviews() { super.layoutSubviews(); _ = layout(width: bounds.width, apply: true) }
  private func layout(width: CGFloat, apply: Bool) -> CGFloat {
    let spacing = theme.spacing
    let inner = max(1, width - 2 * spacing.lg)
    let heading = theme.typography.title2.layout(labels.emptyTitle, width: inner, maxLines: 0, scaled: true, alignment: .center)
    let detail = theme.typography.subhead.layout(labels.emptySubtitle, width: inner, maxLines: 0, scaled: true, alignment: .center)
    var y: CGFloat = 48
    if apply { mark.frame = CGRect(x: (width - 56) / 2, y: y, width: 56, height: 56) }
    y += 56 + spacing.lg
    if apply {
      title.show(heading, theme.colors.label); title.accessibilityLabel = labels.emptyTitle
      title.frame = CGRect(x: spacing.lg, y: y, width: inner, height: heading.size.height)
    }
    y += heading.size.height
    if !labels.emptySubtitle.isEmpty {
      y += spacing.xs
      if apply {
        subtitle.show(detail, theme.colors.secondaryLabel); subtitle.accessibilityLabel = labels.emptySubtitle
        subtitle.frame = CGRect(x: spacing.lg, y: y, width: inner, height: detail.size.height)
      }
      y += detail.size.height
    }
    if apply { subtitle.isHidden = labels.emptySubtitle.isEmpty }
    guard !suggestions.isEmpty else { return y + spacing.xxl }
    y += spacing.xxl
    let columns = ShadowListUtilsPlatform.textScale > 1.4 || inner < 240 ? 1 : 2
    let cardWidth = (inner - CGFloat(columns - 1) * spacing.sm) / CGFloat(columns)
    let direction = ShadowListUtilsDirection(width: width)
    for start in stride(from: 0, to: suggestions.count, by: columns) {
      let end = min(start + columns, suggestions.count)
      let height = suggestions[start..<end].map { item in
        theme.typography.subhead.with(.semibold).layout(item.title, width: cardWidth - 2 * spacing.md).size.height
          + spacing.xxs + theme.typography.footnote.layout(item.prompt, width: cardWidth - 2 * spacing.md, maxLines: 2).size.height + 2 * spacing.md
      }.max() ?? 0
      if apply { for index in start..<end {
        buttons[index].frame = direction.place(CGRect(x: spacing.lg + CGFloat(index - start) * (cardWidth + spacing.sm), y: y, width: cardWidth, height: height))
      } }
      y += height + spacing.sm
    }
    return y - spacing.sm + spacing.xxl
  }
}
