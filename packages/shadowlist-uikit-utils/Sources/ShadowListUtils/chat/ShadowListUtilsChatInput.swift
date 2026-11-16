import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * A multiline composer. nil value uses the initial draft; a non-nil value is application-owned.
 * Sending trims whitespace and only clears an uncontrolled draft.
 */
public final class ShadowListUtilsChatInput: UIView, UITextViewDelegate {
  public var value: String? {
    didSet { editing.value = value; updateSend() }
  }
  public var onChangeText: ((String) -> Void)?
  public var disabled = false { didSet { updateSend() } }
  public var maxLength = 500 { didSet { editing.maxLength = maxLength } }
  public var labels = ShadowListUtilsChatLabels() { didSet { updateSend() } }
  public let theme: ShadowListUtilsTheme
  private let editing: ShadowListUtilsTextInput

  public func textView(_ textView: UITextView, shouldChangeTextIn range: NSRange, replacementText text: String) -> Bool {
    editing.textView(textView, shouldChangeTextIn: range, replacementText: text)
  }

  public var onSend: ((String) -> Void)?
  var maximumInputHeight: CGFloat = 106
  var showsSendButton = true {
    didSet {
      send.isHidden = !showsSendButton
      sendWidth.constant = showsSendButton ? 32 : 0
      sendGap.constant = showsSendButton ? -8 : 0
    }
  }
  private var boxTop: NSLayoutConstraint!
  private var boxBottom: NSLayoutConstraint!
  private var boxLeading: NSLayoutConstraint!
  private var sendTrailing: NSLayoutConstraint!
  private var inputTop: NSLayoutConstraint!
  private var inputBottom: NSLayoutConstraint!
  private var sendWidth: NSLayoutConstraint!
  private var sendGap: NSLayoutConstraint!
  private let box = UIView()
  private let border = UIView()
  let input: ShadowListUtilsTextInputView
  private let placeholder = UILabel()
  let send = UIButton(type: .custom)
  private var inputHeight: NSLayoutConstraint!
  private var embedded = false

  /*
   * The space under the box: the safe area, or spacing.sm where there is none, as above a keyboard.
   */
  private var bottomPadding: CGFloat {
    embedded ? 0 : safeAreaInsets.bottom > 0 ? safeAreaInsets.bottom : theme.spacing.sm
  }

  public init(defaultValue: String = "", theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    let input = ShadowListUtilsTextInputView(frame: .zero, textContainer: nil)
    self.input = input
    editing = ShadowListUtilsTextInput(input: .textView(input), defaultValue: defaultValue)
    super.init(frame: .zero)
    backgroundColor = theme.colors.background
    border.backgroundColor = theme.colors.separator
    box.layer.cornerRadius = theme.radius.lg + 2
    box.layer.borderWidth = ShadowListUtilsTheme.hairline
    box.layer.borderColor = theme.colors.separator.cgColor
    input.font = UIFont.systemFont(ofSize: theme.typography.body.fontSize * ShadowListUtilsPlatform.textScale)
    input.textColor = theme.colors.label
    input.backgroundColor = .clear
    input.textContainerInset = .zero
    input.textContainer.lineFragmentPadding = 0
    editing.maxLength = maxLength
    input.accessibilityLabel = labels.placeholder
    send.setImage(ShadowListUtilsArrowUpIcon.image(size: 20, strokeWidth: 2.4), for: .normal)
    send.layer.cornerRadius = 16
    send.adjustsImageWhenDisabled = false
    send.addTarget(self, action: #selector(sendTapped), for: .touchUpInside)
    for subview in [border, box, input, placeholder, send] {
      subview.translatesAutoresizingMaskIntoConstraints = false
      addSubview(subview)
    }
    inputHeight = input.heightAnchor.constraint(equalToConstant: 22)
    sendWidth = send.widthAnchor.constraint(equalToConstant: 32)
    sendGap = box.trailingAnchor.constraint(equalTo: send.leadingAnchor, constant: -8)
    boxTop = box.topAnchor.constraint(equalTo: topAnchor, constant: theme.spacing.sm + ShadowListUtilsTheme.hairline)
    boxLeading = box.leadingAnchor.constraint(equalTo: leadingAnchor, constant: theme.spacing.sm)
    sendTrailing = send.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -theme.spacing.sm)
    boxBottom = box.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -bottomPadding)
    inputTop = input.topAnchor.constraint(equalTo: box.topAnchor, constant: Self.inputPaddingVertical + ShadowListUtilsTheme.hairline)
    inputBottom = input.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -Self.inputPaddingVertical - ShadowListUtilsTheme.hairline)
    NSLayoutConstraint.activate([
      border.topAnchor.constraint(equalTo: topAnchor),
      border.leadingAnchor.constraint(equalTo: leadingAnchor),
      border.trailingAnchor.constraint(equalTo: trailingAnchor),
      border.heightAnchor.constraint(equalToConstant: ShadowListUtilsTheme.hairline),
      boxTop,
      boxLeading,
      sendGap,
      boxBottom,
      inputTop,
      inputBottom,
      input.leadingAnchor.constraint(equalTo: box.leadingAnchor, constant: 12),
      input.trailingAnchor.constraint(equalTo: box.trailingAnchor, constant: -12),
      inputHeight,
      placeholder.leadingAnchor.constraint(equalTo: input.leadingAnchor),
      placeholder.centerYAnchor.constraint(equalTo: input.centerYAnchor),
      sendWidth,
      send.heightAnchor.constraint(equalToConstant: 32),
      sendTrailing,
      send.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -2),
    ])
    editing.onChangeText = { [weak self] in self?.onChangeText?($0) }
    editing.onContentSizeChange = { [weak self] in self?.applyInputAttributes(); self?.updateInputHeight(); self?.updateSend() }
    applyInputAttributes()
    updateSend()
  }

  public required init?(coder: NSCoder) { fatalError() }

  public func textViewDidChange(_ textView: UITextView) { editing.textViewDidChange(textView) }

  /*
   * The assistant supplies one rounded surface around the input and its toolbar.
   */
  func useEmbeddedAppearance() {
    embedded = true
    boxBottom.constant = 0
    backgroundColor = .clear
    border.isHidden = true
    box.layer.borderWidth = 0
    boxTop.constant = 0
    boxLeading.constant = 0
    sendTrailing.constant = 0
    inputTop.constant = Self.inputPaddingVertical
    inputBottom.constant = -Self.inputPaddingVertical
  }

  public override func safeAreaInsetsDidChange() {
    super.safeAreaInsetsDidChange()
    boxBottom?.constant = -bottomPadding
  }

  /*
   * Typed text keeps the line of the style without the half-leading, as the placeholder does.
   * An empty input forgets its typing attributes.
   */
  private func applyInputAttributes() {
    guard input.text.isEmpty else { return }
    input.typingAttributes = theme.typography.body.attributes(color: theme.colors.label, lineBreakMode: .byWordWrapping, halfLeading: false)
  }

  private func updateInputHeight() {
    let height = min(maximumInputHeight, max(22, ceil(input.sizeThatFits(CGSize(width: input.bounds.width, height: CGFloat.greatestFiniteMagnitude)).height)))
    inputHeight.constant = height
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    guard input.bounds.width > 0 else { return }
    let height = min(maximumInputHeight, max(22, ceil(input.sizeThatFits(CGSize(width: input.bounds.width, height: .greatestFiniteMagnitude)).height)))
    if inputHeight.constant != height { inputHeight.constant = height }
  }

  private func updateSend() {
    let enabled = !disabled && !ShadowListUtilsCore.trim(input.text).isEmpty
    input.isEditable = !disabled
    input.accessibilityLabel = labels.placeholder
    send.accessibilityLabel = labels.send
    placeholder.attributedText = NSAttributedString(
      string: labels.placeholder, attributes: theme.typography.body.attributes(color: theme.colors.secondaryLabel, halfLeading: false))
    placeholder.isHidden = !input.text.isEmpty
    send.backgroundColor = enabled ? theme.colors.accent : theme.colors.fill
    send.tintColor = enabled ? theme.colors.onAccent : theme.colors.secondaryLabel
    send.isEnabled = enabled
  }

  @objc func sendTapped() {
    let text = ShadowListUtilsCore.trim(input.text)
    guard !disabled, !text.isEmpty else { return }
    let controlled = value != nil
    onSend?(text)
    if !controlled {
      editing.clearDraft()
      inputHeight.constant = 22
    }
    updateSend()
  }

  private static let inputPaddingVertical: CGFloat = 7
}
