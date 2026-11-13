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
    didSet { input.text = value ?? draft; updateSend() }
  }
  public var onChangeText: ((String) -> Void)?
  public var disabled = false { didSet { updateSend() } }
  public var maxLength = 500
  public var labels = ShadowListUtilsChatLabels() { didSet { updateSend() } }
  public let theme: ShadowListUtilsTheme
  private var draft: String

  public func textView(_ textView: UITextView, shouldChangeTextIn range: NSRange, replacementText text: String) -> Bool {
    guard !disabled, let current = textView.text else { return false }
    let previous = current as NSString
    guard range.location <= previous.length, range.length <= previous.length - range.location else { return false }
    let replacement = text as NSString
    var allowed = max(0, maxLength - previous.length + range.length)
    if replacement.length <= allowed { return true }
    // Match RN's native iOS input: a long paste keeps the fitting prefix, never half a composed character.
    guard replacement.length > 1, allowed > 0 else { return false }
    let cutoff = replacement.rangeOfComposedCharacterSequence(at: allowed - 1)
    if NSMaxRange(cutoff) > allowed { allowed = cutoff.location }
    guard allowed > 0 else { return false }
    textView.text = previous.replacingCharacters(in: range, with: replacement.substring(to: allowed))
    textView.selectedRange = NSRange(location: range.location + allowed, length: 0)
    textViewDidChange(textView)
    return false
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
  let input = UITextView()
  private let placeholder = UILabel()
  let send = UIButton(type: .system)
  private var inputHeight: NSLayoutConstraint!

  public init(defaultValue: String = "", theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    draft = defaultValue
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
    input.delegate = self
    input.text = draft
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
    updateSend()
  }

  public required init?(coder: NSCoder) { fatalError() }

  public func textViewDidChange(_ textView: UITextView) {
    let next = textView.text ?? ""
    if value == nil { draft = next }
    onChangeText?(next)
    if value != nil { input.text = value ?? draft }
    let height = min(maximumInputHeight, max(22, ceil(textView.sizeThatFits(CGSize(width: textView.bounds.width, height: .greatestFiniteMagnitude)).height)))
    inputHeight.constant = height
    updateSend()
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
      draft = ""
      input.text = value ?? draft
      inputHeight.constant = 22
    }
    updateSend()
  }
}
