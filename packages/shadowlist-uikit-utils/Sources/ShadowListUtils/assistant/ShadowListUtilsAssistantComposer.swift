import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * The existing Assistant composer contract, using the native chat input's editing engine.
 * All application options and attachments stay controlled; draft requests go through onChangeText.
 */
public final class ShadowListUtilsAssistantComposer: UIView {
  public var value: String? { didSet { input.value = value; updatePrimary() } }
  public var onChangeText: ((String) -> Void)?
  public var onSend: ((String, [ShadowListUtilsAssistantAttachment]) -> Void)? { didSet { updatePrimary() } }
  public var streaming = false { didSet { updatePrimary() } }
  public var onStop: (() -> Void)? { didSet { updatePrimary() } }
  public var sendWhileStreaming = false { didSet { updatePrimary() } }
  public var attachments: [ShadowListUtilsAssistantAttachment] = [] { didSet { rebuildTray(); updatePrimary() } }
  public var onPressAttach: (() -> Void)? { didSet { updateToolbar() } }
  public var onRemoveAttachment: ((String) -> Void)? { didSet { rebuildTray() } }
  public var model: String? { didSet { updateToolbar() } }
  public var onPressModel: (() -> Void)? { didSet { updateToolbar() } }
  public var thinking = false { didSet { updateToolbar() } }
  public var onThinkingChange: ((Bool) -> Void)? { didSet { updateToolbar() } }
  public var accessory: UIView? {
    didSet {
      oldValue?.removeFromSuperview()
      if let accessory { toolbar.insertArrangedSubview(accessory, at: 3) }
    }
  }
  public var dictating = false { didSet { updateToolbar() } }
  public var onPressDictate: (() -> Void)? { didSet { updateToolbar() } }
  public var editing = false { didSet { editBanner.isHidden = !editing } }
  public var onCancelEdit: (() -> Void)?
  public var maxLength = 4000 { didSet { input.maxLength = maxLength } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { applyLabels(); rebuildTray(); updateToolbar() } }
  public let theme: ShadowListUtilsTheme
  let input: ShadowListUtilsChatInput
  let primary = UIButton(type: .system)
  private let stack = UIStackView()
  private let editBanner = UIStackView()
  private let editLabel = UILabel()
  private let cancel = UIButton(type: .system)
  private let tray = UIScrollView()
  private let trayContent = UIStackView()
  private var trayHeight: NSLayoutConstraint!
  private let toolbar = UIStackView()
  private let attach = UIButton(type: .system)
  private let modelButton = UIButton(type: .system)
  private let thinkingSwitch = UISwitch()
  private let spacer = UIView()
  private let dictate = UIButton(type: .system)

  public init(theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    input = ShadowListUtilsChatInput(theme: theme)
    super.init(frame: .zero)
    backgroundColor = theme.colors.background
    input.showsSendButton = false
    input.maximumInputHeight = 126
    input.maxLength = maxLength
    input.onChangeText = { [weak self] text in self?.onChangeText?(text); self?.updatePrimary() }
    stack.axis = .vertical
    stack.spacing = 0
    stack.translatesAutoresizingMaskIntoConstraints = false
    addSubview(stack)
    editBanner.axis = .horizontal
    editBanner.spacing = theme.spacing.sm
    editBanner.addArrangedSubview(editLabel)
    editBanner.addArrangedSubview(cancel)
    editBanner.isHidden = true
    cancel.setImage(UIImage(systemName: "xmark"), for: .normal)
    cancel.addAction(UIAction { [weak self] _ in self?.onCancelEdit?() }, for: .touchUpInside)
    stack.addArrangedSubview(editBanner)
    tray.showsHorizontalScrollIndicator = false
    trayContent.axis = .horizontal
    trayContent.spacing = theme.spacing.sm
    tray.addSubview(trayContent)
    trayHeight = tray.heightAnchor.constraint(equalToConstant: 0)
    trayHeight.isActive = true
    stack.addArrangedSubview(tray)
    stack.addArrangedSubview(input)
    toolbar.axis = .horizontal
    toolbar.alignment = .center
    toolbar.spacing = theme.spacing.xs
    [attach, modelButton, thinkingSwitch, spacer, dictate, primary].forEach(toolbar.addArrangedSubview)
    attach.setImage(UIImage(systemName: "plus"), for: .normal)
    attach.addAction(UIAction { [weak self] _ in self?.onPressAttach?() }, for: .touchUpInside)
    modelButton.addAction(UIAction { [weak self] _ in self?.onPressModel?() }, for: .touchUpInside)
    thinkingSwitch.addTarget(self, action: #selector(thinkingChanged), for: .valueChanged)
    thinkingSwitch.onTintColor = theme.colors.accent
    dictate.setImage(UIImage(systemName: "mic"), for: .normal)
    dictate.addAction(UIAction { [weak self] _ in self?.onPressDictate?() }, for: .touchUpInside)
    primary.addTarget(self, action: #selector(submit), for: .touchUpInside)
    for button in [attach, dictate, primary] {
      button.widthAnchor.constraint(equalToConstant: 32).isActive = true
      button.heightAnchor.constraint(equalToConstant: 32).isActive = true
    }
    stack.addArrangedSubview(toolbar)
    NSLayoutConstraint.activate([
      stack.topAnchor.constraint(equalTo: topAnchor, constant: theme.spacing.sm),
      stack.leadingAnchor.constraint(equalTo: leadingAnchor, constant: theme.spacing.sm),
      stack.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -theme.spacing.sm),
      stack.bottomAnchor.constraint(equalTo: safeAreaLayoutGuide.bottomAnchor, constant: -theme.spacing.sm),
      toolbar.heightAnchor.constraint(greaterThanOrEqualToConstant: 32),
    ])
    applyLabels()
    updateToolbar()
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  public func setDraft(_ text: String, focus: Bool = true) {
    input.input.text = text
    input.textViewDidChange(input.input)
    if focus { self.focus() }
  }
  public func getDraft() -> String { value ?? input.input.text ?? "" }
  public func clearDraft() { setDraft("", focus: false) }
  public func focus() { input.input.becomeFirstResponder() }

  private var hasDraft: Bool { !ShadowListUtilsCore.trim(getDraft()).isEmpty || !attachments.isEmpty }
  private var showStop: Bool { streaming && !(sendWhileStreaming && hasDraft) }

  @objc func submit() {
    if showStop { onStop?(); return }
    guard let onSend, hasDraft, !streaming || sendWhileStreaming else { return }
    onSend(ShadowListUtilsCore.trim(getDraft()), attachments)
    clearDraft()
  }

  @objc private func thinkingChanged() {
    let requested = thinkingSwitch.isOn
    thinkingSwitch.setOn(thinking, animated: false)
    onThinkingChange?(requested)
  }

  private func applyLabels() {
    var chat = input.labels
    chat.placeholder = labels.placeholder
    chat.send = labels.send
    input.labels = chat
    editLabel.text = labels.editingMessage
    editLabel.textColor = theme.colors.accent
    cancel.accessibilityLabel = labels.cancelEditing
  }

  private func updatePrimary() {
    primary.setImage(UIImage(systemName: showStop ? "stop.fill" : "arrow.up"), for: .normal)
    primary.accessibilityLabel = showStop ? labels.stop : labels.send
    primary.isEnabled = showStop ? onStop != nil : onSend != nil && hasDraft && (!streaming || sendWhileStreaming)
    primary.tintColor = showStop ? theme.colors.background : primary.isEnabled ? theme.colors.label : theme.colors.secondaryLabel
    primary.backgroundColor = showStop ? theme.colors.label : theme.colors.fill
    primary.layer.cornerRadius = 16
  }

  private func updateToolbar() {
    attach.isHidden = onPressAttach == nil
    attach.accessibilityLabel = labels.addAttachment
    modelButton.isHidden = model == nil
    modelButton.setTitle(model, for: .normal)
    modelButton.titleLabel?.font = .systemFont(ofSize: theme.typography.footnote.fontSize * ShadowListUtilsPlatform.textScale)
    modelButton.accessibilityLabel = model.map(labels.model)
    modelButton.isEnabled = onPressModel != nil
    thinkingSwitch.isHidden = onThinkingChange == nil
    thinkingSwitch.setOn(thinking, animated: false)
    thinkingSwitch.accessibilityLabel = labels.thinkingToggleDescription
    dictate.isHidden = onPressDictate == nil
    dictate.accessibilityLabel = dictating ? labels.stopDictating : labels.dictate
    dictate.accessibilityTraits = dictating ? [.button, .selected] : .button
    updatePrimary()
  }

  private func rebuildTray() {
    trayContent.arrangedSubviews.forEach { trayContent.removeArrangedSubview($0); $0.removeFromSuperview() }
    var x: CGFloat = 0
    var height: CGFloat = 0
    for attachment in attachments {
      let chip = ShadowListUtilsAssistantAttachmentChip(attachment: attachment, theme: theme, onRemove: onRemoveAttachment)
      chip.labels = labels
      let size = chip.sizeThatFits(CGSize(width: 220, height: CGFloat.greatestFiniteMagnitude))
      chip.widthAnchor.constraint(equalToConstant: size.width).isActive = true
      chip.heightAnchor.constraint(equalToConstant: size.height).isActive = true
      trayContent.addArrangedSubview(chip)
      x += size.width + theme.spacing.sm
      height = max(height, size.height)
    }
    trayContent.frame = CGRect(x: 0, y: 0, width: max(0, x - theme.spacing.sm), height: height)
    tray.contentSize = trayContent.frame.size
    trayHeight.constant = attachments.isEmpty ? 0 : height + theme.spacing.sm
  }
}
