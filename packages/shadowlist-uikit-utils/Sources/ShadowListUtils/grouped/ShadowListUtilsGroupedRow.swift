import ShadowListKit
import UIKit

/* A grouped row whose primary press and enclosing swipe/menu surface have separate owners. */
public final class ShadowListUtilsGroupedRow: UIView {
  public var title: String { didSet { refresh() } }
  public var subtitle: String? { didSet { refresh() } }
  public var subtitleLines = 1 { didSet { refresh() } }
  public var leading: UIView? { didSet { refresh() } }
  public var trailing: UIView? { didSet { refresh() } }
  public var chevron: Bool? { didSet { refresh() } }
  public var onPress: (() -> Void)?
  public var onLongPress: (() -> Void)? { didSet { refresh() } }
  public var longPressLabel: String?
  public var disabled = false { didSet { refresh() } }
  public var separated = false { didSet { refresh() } }
  public var accessibilityRole: ShadowListUtilsGroupedRowAccessibilityRole = .button { didSet { refresh() } }
  public var testID: String? { didSet { accessibilityIdentifier = testID } }
  public var theme: ShadowListUtilsTheme { didSet { actions.theme = theme; refresh() } }
  public var leadingSwipeActions: ShadowListKitSwipeActionsConfiguration? { didSet { actions.leadingSwipeActions = leadingSwipeActions } }
  public var trailingSwipeActions: ShadowListKitSwipeActionsConfiguration? { didSet { actions.trailingSwipeActions = trailingSwipeActions } }
  public var contextMenu: UIMenu? { didSet { actions.contextMenu = contextMenu; refresh() } }

  private let primary: ShadowListUtilsGroupedRowContent
  private let row = UIStackView()
  private let body = UIView()
  private let bodyRow = UIStackView()
  private let texts = UIStackView()
  private let titleView = UILabel()
  private let subtitleView = UILabel()
  private let chevronView = ShadowListUtilsIconView(kind: .chevron)
  private let separator = UIView()
  private let actions: ShadowListUtilsGroupedActions
  private var rowInset: NSLayoutConstraint!
  private var rightInset: NSLayoutConstraint!
  private var topInset: NSLayoutConstraint!
  private var bottomInset: NSLayoutConstraint!
  private var minimumHeight: NSLayoutConstraint!
  private var menuOwnsPress = false
  private var layoutWidth: CGFloat = -1
  private lazy var longPress = UILongPressGestureRecognizer(target: self, action: #selector(held(_:)))

  public init(title: String, theme: ShadowListUtilsTheme = .system) {
    self.title = title; self.theme = theme
    let primary = ShadowListUtilsGroupedRowContent(type: .custom)
    self.primary = primary
    actions = ShadowListUtilsGroupedActions(children: [primary], theme: theme)
    super.init(frame: .zero)
    addSubview(actions)
    isAccessibilityElement = true
    primary.isAccessibilityElement = false
    primary.theme = theme
    row.axis = .horizontal; row.alignment = .center
    bodyRow.axis = .horizontal; bodyRow.alignment = .center
    texts.axis = .vertical; texts.spacing = 1
    row.translatesAutoresizingMaskIntoConstraints = false
    bodyRow.translatesAutoresizingMaskIntoConstraints = false
    separator.translatesAutoresizingMaskIntoConstraints = false
    primary.addSubview(row); body.addSubview(bodyRow); body.addSubview(separator)
    rowInset = row.leadingAnchor.constraint(equalTo: primary.leadingAnchor)
    rightInset = bodyRow.trailingAnchor.constraint(equalTo: body.trailingAnchor)
    topInset = bodyRow.topAnchor.constraint(equalTo: body.topAnchor)
    bottomInset = bodyRow.bottomAnchor.constraint(equalTo: body.bottomAnchor)
    minimumHeight = body.heightAnchor.constraint(greaterThanOrEqualToConstant: theme.grouped.rowHeight)
    NSLayoutConstraint.activate([
      row.topAnchor.constraint(equalTo: primary.topAnchor), row.bottomAnchor.constraint(equalTo: primary.bottomAnchor),
      rowInset, row.trailingAnchor.constraint(equalTo: primary.trailingAnchor),
      bodyRow.leadingAnchor.constraint(equalTo: body.leadingAnchor), rightInset, topInset, bottomInset, minimumHeight,
      separator.topAnchor.constraint(equalTo: body.topAnchor), separator.leadingAnchor.constraint(equalTo: body.leadingAnchor),
      separator.trailingAnchor.constraint(equalTo: body.trailingAnchor), separator.heightAnchor.constraint(equalToConstant: ShadowListUtilsTheme.hairline),
      chevronView.widthAnchor.constraint(equalToConstant: 16), chevronView.heightAnchor.constraint(equalToConstant: 16),
    ])
    texts.setContentHuggingPriority(.defaultLow, for: .horizontal)
    texts.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
    titleView.lineBreakMode = .byTruncatingTail; subtitleView.lineBreakMode = .byTruncatingTail
    primary.addAction(UIAction { [weak self] _ in self?.menuOwnsPress = false }, for: .touchDown)
    primary.addAction(UIAction { [weak self] _ in
      guard let self, !self.disabled, !self.menuOwnsPress else { return }
      self.onPress?()
    }, for: .touchUpInside)
    primary.addGestureRecognizer(longPress)
    actions.onContextMenuWillShow = { [weak self] in self?.menuOwnsPress = true; self?.primary.isHighlighted = false }
    refresh()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  public func closeSwipeActions(animated: Bool = true) { actions.closeSwipeActions(animated: animated) }
  public override func accessibilityActivate() -> Bool {
    guard !disabled else { return false }
    onPress?(); return true
  }
  public override var accessibilityCustomActions: [UIAccessibilityCustomAction]? {
    get {
      var current = super.accessibilityCustomActions ?? []
      if onLongPress != nil, let label = longPressLabel {
        current.append(UIAccessibilityCustomAction(name: label, actionHandler: { [weak self] _ in
          guard let callback = self?.onLongPress else { return false }
          callback(); return true
        }))
      }
      return current + actions.swipeAccessibilityActions
    }
    set { super.accessibilityCustomActions = newValue }
  }
  public override var intrinsicContentSize: CGSize {
    CGSize(width: UIView.noIntrinsicMetric, height: sizeThatFits(CGSize(width: bounds.width, height: CGFloat.greatestFiniteMagnitude)).height)
  }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { actions.sizeThatFits(size) }
  public override func layoutSubviews() {
    super.layoutSubviews(); actions.frame = bounds
    if layoutWidth != bounds.width { layoutWidth = bounds.width; invalidateIntrinsicContentSize() }
  }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window)
      refresh()
    }
  }
  @objc private func held(_ recognizer: UILongPressGestureRecognizer) {
    if recognizer.state == .began, !disabled { onLongPress?() }
  }
  private func refresh() {
    let large = ShadowListUtilsPlatform.isLargeText
    titleView.numberOfLines = large ? 3 : 1
    subtitleView.numberOfLines = large ? max(3, subtitleLines) : max(0, subtitleLines)
    titleView.attributedText = attributed(title, style: theme.typography.body, color: theme.colors.label)
    subtitleView.attributedText = subtitle.map { attributed($0, style: theme.typography.footnote, color: theme.colors.secondaryLabel) }
    texts.replaceArrangedSubviews(subtitle != nil ? [titleView, subtitleView] : [titleView])
    bodyRow.replaceArrangedSubviews([texts] + (trailing.map { [$0] } ?? []) + ((chevron ?? theme.grouped.chevron) ? [chevronView] : []))
    row.replaceArrangedSubviews((leading.map { [$0] } ?? []) + [body])
    row.spacing = theme.spacing.md; bodyRow.spacing = theme.spacing.sm
    rowInset.constant = theme.grouped.rowInset; rightInset.constant = -theme.grouped.rowInset
    topInset.constant = theme.spacing.sm + 2; bottomInset.constant = -(theme.spacing.sm + 2)
    minimumHeight.constant = theme.grouped.rowHeight
    chevronView.color = theme.colors.tertiaryLabel
    separator.isHidden = !separated; separator.backgroundColor = theme.colors.separator
    primary.theme = theme; primary.isEnabled = !disabled; primary.alpha = disabled ? 0.4 : 1
    longPress.isEnabled = onLongPress != nil && !disabled && (contextMenu == nil || contextMenu?.children.isEmpty == true)
    accessibilityLabel = subtitle.map { "\(title), \($0)" } ?? title
    accessibilityTraits = accessibilityRole == .button ? .button : .link
    if disabled { accessibilityTraits.insert(.notEnabled) }
    invalidateIntrinsicContentSize(); setNeedsLayout()
  }
  private func attributed(_ text: String, style: ShadowListUtilsTextStyle, color: UIColor) -> NSAttributedString {
    let scale = ShadowListUtilsPlatform.textScale
    let paragraph = NSMutableParagraphStyle()
    paragraph.minimumLineHeight = style.scaledLineHeight(); paragraph.maximumLineHeight = paragraph.minimumLineHeight
    paragraph.lineBreakMode = .byTruncatingTail
    return NSAttributedString(string: text, attributes: [.font: UIFont.systemFont(ofSize: style.fontSize * scale, weight: style.weight),
      .kern: style.letterSpacing * scale, .foregroundColor: color, .paragraphStyle: paragraph])
  }
}
