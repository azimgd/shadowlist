import UIKit

/* Controlled field or code/text block; only native text input owns marked text and selection. */
public final class ShadowListUtilsTextField: UIView {
  public var value: String { didSet { editing.value = value; updatePlaceholder() } }
  public var onChange: ((String) -> Void)?
  public var placeholder: String? { didSet { updatePlaceholder() } }
  public var keyboardType: UIKeyboardType = .default { didSet { field.keyboardType = keyboardType; textView.keyboardType = keyboardType } }
  public var multiline = false { didSet { if multiline != oldValue { updateMode() } } }
  public let theme: ShadowListUtilsTheme
  let field = ShadowListUtilsTextInputField()
  let textView = ShadowListUtilsTextInputView()
  private let placeholderView = UILabel()
  private lazy var editing = ShadowListUtilsTextInput(input: .textField(field), defaultValue: value)

  public init(value: String, theme: ShadowListUtilsTheme = .system) {
    self.value = value; self.theme = theme
    super.init(frame: .zero)
    backgroundColor = theme.colors.fill; layer.cornerRadius = 9
    applyTypography()
    textView.backgroundColor = .clear; textView.textContainerInset = .zero; textView.textContainer.lineFragmentPadding = 0
    field.textColor = theme.colors.label; textView.textColor = theme.colors.label
    field.autocorrectionType = .no; textView.autocorrectionType = .no
    field.autocapitalizationType = .none; textView.autocapitalizationType = .none
    placeholderView.textColor = theme.colors.secondaryLabel
    placeholderView.font = textView.font; placeholderView.isUserInteractionEnabled = false; placeholderView.isAccessibilityElement = false
    addSubview(field); addSubview(textView); addSubview(placeholderView)
    editing.value = value
    editing.onChangeText = { [weak self] in self?.onChange?($0) }
    editing.onContentSizeChange = { [weak self] in self?.updatePlaceholder() }
    updateMode()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var accessibilityLabel: String? {
    get { super.accessibilityLabel }
    set { super.accessibilityLabel = newValue; field.accessibilityLabel = newValue ?? placeholder; textView.accessibilityLabel = newValue ?? placeholder }
  }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: multiline ? 104 : 40) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: intrinsicContentSize.height) }
  public override func layoutSubviews() {
    super.layoutSubviews()
    field.frame = bounds.insetBy(dx: 12, dy: 0)
    textView.frame = bounds.insetBy(dx: 12, dy: 10)
    placeholderView.frame = CGRect(x: 12, y: 10, width: max(0, bounds.width - 24), height: placeholderView.font.lineHeight)
  }
  public override var canBecomeFirstResponder: Bool { multiline ? textView.canBecomeFirstResponder : field.canBecomeFirstResponder }
  public override func becomeFirstResponder() -> Bool { multiline ? textView.becomeFirstResponder() : field.becomeFirstResponder() }
  public override func resignFirstResponder() -> Bool { multiline ? textView.resignFirstResponder() : field.resignFirstResponder() }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window)
      applyTypography()
    }
  }
  private func applyTypography() {
    let scale = ShadowListUtilsPlatform.textScale
    field.font = .systemFont(ofSize: theme.typography.body.fontSize * scale)
    field.defaultTextAttributes[.kern] = theme.typography.body.letterSpacing * scale
    let font = UIFont.monospacedSystemFont(ofSize: 14 * scale, weight: .regular)
    textView.font = font
    textView.typingAttributes = [.font: font, .kern: -0.6 * scale, .foregroundColor: theme.colors.label]
    placeholderView.font = font
    setNeedsLayout()
  }
  private func updateMode() {
    let focused = field.isFirstResponder || textView.isFirstResponder
    field.resignFirstResponder(); textView.resignFirstResponder()
    field.isHidden = multiline; textView.isHidden = !multiline
    editing.bind(multiline ? .textView(textView) : .textField(field))
    if focused { _ = becomeFirstResponder() }
    updatePlaceholder(); invalidateIntrinsicContentSize(); setNeedsLayout()
  }
  private func updatePlaceholder() {
    field.accessibilityLabel = super.accessibilityLabel ?? placeholder
    field.attributedPlaceholder = placeholder.map { NSAttributedString(string: $0, attributes: [.foregroundColor: theme.colors.secondaryLabel]) }
    placeholderView.text = placeholder
    placeholderView.isHidden = !multiline || !editing.text.isEmpty
    textView.accessibilityLabel = super.accessibilityLabel ?? placeholder
  }
}
