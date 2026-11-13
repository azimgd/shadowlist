import UIKit

/* A controlled, borderless form field. Its row owns the box and stacked alignment. */
public final class ShadowListUtilsFormInput: UIView {
  public var value: String { didSet { editing.value = value } }
  public var onChange: ((String) -> Void)?
  public var placeholder: String? { didSet { applyConfiguration() } }
  public var numeric = false { didSet { applyConfiguration() } }
  public var keyboardType: UIKeyboardType? { didSet { applyConfiguration() } }
  public var autoCapitalize: UITextAutocapitalizationType? { didSet { applyConfiguration() } }
  public var autoCorrect: Bool? { didSet { applyConfiguration() } }
  public var returnKeyType: UIReturnKeyType? { didSet { applyConfiguration() } }
  public var onSubmitEditing: (() -> Void)?
  public let theme: ShadowListUtilsTheme
  var stacked = false { didSet { input.textAlignment = stacked ? .left : .right } }
  let input = ShadowListUtilsTextInputField()
  private lazy var editing = ShadowListUtilsTextInput(input: .textField(input), defaultValue: value)

  public init(value: String, accessibilityLabel: String, theme: ShadowListUtilsTheme = .system) {
    self.value = value; self.theme = theme
    super.init(frame: .zero)
    addSubview(input)
    input.accessibilityLabel = accessibilityLabel
    input.clearButtonMode = .whileEditing
    editing.value = value
    editing.onChangeText = { [weak self] in self?.onChange?($0) }
    editing.onSubmitEditing = { [weak self] in self?.onSubmitEditing?() }
    editing.onContentSizeChange = { [weak self] in self?.invalidateIntrinsicContentSize() }
    applyConfiguration()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var accessibilityLabel: String? {
    get { input.accessibilityLabel }
    set { input.accessibilityLabel = newValue }
  }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: max(38, input.font?.lineHeight ?? 0)) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: intrinsicContentSize.height) }
  public override func layoutSubviews() { super.layoutSubviews(); input.frame = bounds }
  public override var canBecomeFirstResponder: Bool { input.canBecomeFirstResponder }
  public override func becomeFirstResponder() -> Bool { input.becomeFirstResponder() }
  public override func resignFirstResponder() -> Bool { input.resignFirstResponder() }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window)
      applyConfiguration()
    }
  }
  private func applyConfiguration() {
    let literal = numeric || keyboardType == .emailAddress
    let nextKeyboard = keyboardType ?? (numeric ? UIKeyboardType.decimalPad : .default)
    let nextCapitalization = autoCapitalize ?? (literal ? UITextAutocapitalizationType.none : .sentences)
    let nextCorrection: UITextAutocorrectionType = (autoCorrect ?? !literal) ? .yes : .no
    let nextReturn = returnKeyType ?? .default
    let traitsChanged = input.keyboardType != nextKeyboard || input.autocapitalizationType != nextCapitalization || input.autocorrectionType != nextCorrection || input.returnKeyType != nextReturn
    input.keyboardType = nextKeyboard
    input.autocapitalizationType = nextCapitalization
    input.autocorrectionType = nextCorrection
    input.returnKeyType = nextReturn
    input.textAlignment = stacked ? .left : .right
    let size = theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale
    let font = numeric ? UIFont.monospacedDigitSystemFont(ofSize: size, weight: .regular) : UIFont.systemFont(ofSize: size)
    if input.font != font { input.font = font }
    input.defaultTextAttributes[.kern] = theme.typography.body.letterSpacing * ShadowListUtilsPlatform.textScale
    input.textColor = theme.colors.label
    input.attributedPlaceholder = placeholder.map { NSAttributedString(string: $0, attributes: [.foregroundColor: theme.colors.tertiaryLabel, .font: font]) }
    if traitsChanged && input.isFirstResponder { input.reloadInputViews() }
    invalidateIntrinsicContentSize()
  }
}
