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
  private var sendWidth: NSLayoutConstraint!
  private var sendGap: NSLayoutConstraint!
  private let box = UIView()
  let input: ShadowListUtilsTextInputView
  private let placeholder = UILabel()
  let send = UIButton(type: .system)
  private var inputHeight: NSLayoutConstraint!

  public init(defaultValue: String = "", theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    let input = ShadowListUtilsTextInputView(frame: .zero, textContainer: nil)
    self.input = input
    editing = ShadowListUtilsTextInput(input: .textView(input), defaultValue: defaultValue)
    super.init(frame: .zero)
    backgroundColor = theme.colors.background
    let border = UIView()
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
    placeholder.font = input.font
    placeholder.textColor = theme.colors.secondaryLabel
    placeholder.text = labels.placeholder
    send.setImage(UIImage(systemName: "arrow.up", withConfiguration: UIImage.SymbolConfiguration(pointSize: 16, weight: .bold)), for: .normal)
    send.layer.cornerRadius = 16
    send.addTarget(self, action: #selector(sendTapped), for: .touchUpInside)
    for subview in [border, box, input, placeholder, send] {
      subview.translatesAutoresizingMaskIntoConstraints = false
      addSubview(subview)
    }
    inputHeight = input.heightAnchor.constraint(equalToConstant: 22)
    sendWidth = send.widthAnchor.constraint(equalToConstant: 32)
    sendGap = box.trailingAnchor.constraint(equalTo: send.leadingAnchor, constant: -8)
    NSLayoutConstraint.activate([
      border.topAnchor.constraint(equalTo: topAnchor),
      border.leadingAnchor.constraint(equalTo: leadingAnchor),
      border.trailingAnchor.constraint(equalTo: trailingAnchor),
      border.heightAnchor.constraint(equalToConstant: ShadowListUtilsTheme.hairline),
      box.topAnchor.constraint(equalTo: topAnchor, constant: 8),
      box.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 8),
      sendGap,
      box.bottomAnchor.constraint(equalTo: safeAreaLayoutGuide.bottomAnchor, constant: -8),
      input.topAnchor.constraint(equalTo: box.topAnchor, constant: 7),
      input.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -7),
      input.leadingAnchor.constraint(equalTo: box.leadingAnchor, constant: 12),
      input.trailingAnchor.constraint(equalTo: box.trailingAnchor, constant: -12),
      inputHeight,
      placeholder.leadingAnchor.constraint(equalTo: input.leadingAnchor),
      placeholder.centerYAnchor.constraint(equalTo: input.centerYAnchor),
      sendWidth,
      send.heightAnchor.constraint(equalToConstant: 32),
      send.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -8),
      send.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -2),
    ])
    editing.onChangeText = { [weak self] in self?.onChangeText?($0) }
    editing.onContentSizeChange = { [weak self] in self?.updateInputHeight(); self?.updateSend() }
    updateSend()
  }

  public required init?(coder: NSCoder) { fatalError() }

  public func textViewDidChange(_ textView: UITextView) { editing.textViewDidChange(textView) }

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
    placeholder.text = labels.placeholder
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
}
