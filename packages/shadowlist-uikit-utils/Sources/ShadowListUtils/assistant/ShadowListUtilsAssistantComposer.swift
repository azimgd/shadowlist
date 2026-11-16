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
  let primary = UIButton(type: .custom)
  private let stack = UIStackView()
  private let field = UIView()
  private let border = UIView()
  private let editBanner = UIStackView()
  private let editLabel = UILabel()
  private let cancel = UIButton(type: .system)
  private let tray = UIScrollView()
  private let trayContent = UIStackView()
  private var trayHeight: NSLayoutConstraint!
  private var stackBottom: NSLayoutConstraint!
  private var modelWidth: NSLayoutConstraint!
  private var thinkingWidth: NSLayoutConstraint!
  private let toolbar = UIStackView()
  private let attach = UIButton(type: .system)
  private let modelButton = UIButton(type: .system)
  private let thinkingButton = UIButton(type: .system)
  private let spacer = UIView()
  private let dictate = UIButton(type: .system)

  public init(theme: ShadowListUtilsTheme = .system) {
    self.theme = theme
    input = ShadowListUtilsChatInput(theme: theme)
    super.init(frame: .zero)
    backgroundColor = theme.colors.background
    input.showsSendButton = false
    input.useEmbeddedAppearance()
    field.backgroundColor = theme.colors.elevated
    field.layer.cornerRadius = theme.radius.xl
    field.layer.borderWidth = ShadowListUtilsTheme.hairline
    field.layer.borderColor = theme.colors.separator.cgColor
    addSubview(field)
    border.backgroundColor = theme.colors.separator
    addSubview(border)
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
    toolbar.isLayoutMarginsRelativeArrangement = true
    toolbar.layoutMargins = UIEdgeInsets(top: 0, left: theme.spacing.xs + 2, bottom: theme.spacing.xs + 2, right: theme.spacing.xs + 2)
    [attach, modelButton, thinkingButton, spacer, dictate, primary].forEach(toolbar.addArrangedSubview)
    attach.setImage(ShadowListUtilsPlusIcon.image(size: 18), for: .normal)
    attach.tintColor = theme.colors.label
    attach.addAction(UIAction { [weak self] _ in self?.onPressAttach?() }, for: .touchUpInside)
    modelButton.addAction(UIAction { [weak self] _ in self?.onPressModel?() }, for: .touchUpInside)
    thinkingButton.addTarget(self, action: #selector(thinkingChanged), for: .touchUpInside)
    modelButton.heightAnchor.constraint(equalToConstant: 30).isActive = true
    thinkingButton.heightAnchor.constraint(equalToConstant: 30).isActive = true
    modelWidth = modelButton.widthAnchor.constraint(equalToConstant: 0)
    thinkingWidth = thinkingButton.widthAnchor.constraint(equalToConstant: 0)
    modelWidth.isActive = true
    thinkingWidth.isActive = true
    dictate.setImage(UIImage(systemName: "mic"), for: .normal)
    dictate.addAction(UIAction { [weak self] _ in self?.onPressDictate?() }, for: .touchUpInside)
    primary.addTarget(self, action: #selector(submit), for: .touchUpInside)
    for button in [attach, dictate, primary] {
      button.widthAnchor.constraint(equalToConstant: 32).isActive = true
      button.heightAnchor.constraint(equalToConstant: 32).isActive = true
    }
    stack.addArrangedSubview(toolbar)
    stackBottom = stack.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -bottomPadding - ShadowListUtilsTheme.hairline)
    NSLayoutConstraint.activate([
      stack.topAnchor.constraint(equalTo: topAnchor, constant: 2 * (theme.spacing.sm + ShadowListUtilsTheme.hairline)),
      stack.leadingAnchor.constraint(equalTo: leadingAnchor, constant: theme.spacing.sm + ShadowListUtilsTheme.hairline),
      stack.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -theme.spacing.sm - ShadowListUtilsTheme.hairline),
      stackBottom,
      toolbar.heightAnchor.constraint(greaterThanOrEqualToConstant: 32),
    ])
    applyLabels()
    updateToolbar()
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  /*
   * The space under the field: the safe area, or spacing.sm where there is none, as above a keyboard.
   */
  private var bottomPadding: CGFloat {
    safeAreaInsets.bottom > 0 ? safeAreaInsets.bottom : theme.spacing.sm
  }

  public override func safeAreaInsetsDidChange() {
    super.safeAreaInsetsDidChange()
    stackBottom?.constant = -bottomPadding - ShadowListUtilsTheme.hairline
    setNeedsLayout()
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    let top = theme.spacing.sm + ShadowListUtilsTheme.hairline
    field.frame = CGRect(x: theme.spacing.sm, y: top, width: bounds.width - 2 * theme.spacing.sm, height: bounds.height - bottomPadding - top)
    border.frame = CGRect(x: 0, y: 0, width: bounds.width, height: ShadowListUtilsTheme.hairline)
  }

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
    onThinkingChange?(!thinking)
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
    primary.setImage(showStop ? UIImage(systemName: "stop.fill") : ShadowListUtilsArrowUpIcon.image(size: 18, strokeWidth: 2.4), for: .normal)
    primary.accessibilityLabel = showStop ? labels.stop : labels.send
    primary.adjustsImageWhenDisabled = false
    primary.isEnabled = showStop ? onStop != nil : onSend != nil && hasDraft && (!streaming || sendWhileStreaming)
    primary.tintColor = showStop ? theme.colors.background : primary.isEnabled ? theme.colors.label : theme.colors.secondaryLabel
    primary.backgroundColor = showStop ? theme.colors.label : primary.isEnabled ? theme.colors.accent : theme.colors.fill
    primary.layer.cornerRadius = 16
  }

  private func updateToolbar() {
    attach.isHidden = onPressAttach == nil
    attach.accessibilityLabel = labels.addAttachment
    modelButton.isHidden = model == nil
    let modelTitle = model.map { theme.typography.footnote.with(.semibold).attributedString($0, color: theme.colors.label) }
    modelButton.setAttributedTitle(modelTitle, for: .normal)
    modelButton.setImage(onPressModel == nil ? nil : ShadowListUtilsChevronIcon.image(size: Self.modelChevronSize, strokeWidth: 1.8, direction: .down), for: .normal)
    modelWidth.constant = pillWidth(modelTitle) + (onPressModel == nil ? 0 : theme.spacing.xs + Self.modelChevronSize)
    modelButton.tintColor = theme.colors.secondaryLabel
    modelButton.semanticContentAttribute = .forceRightToLeft
    modelButton.contentHorizontalAlignment = .left
    modelButton.imageEdgeInsets = UIEdgeInsets(top: 0, left: theme.spacing.xs, bottom: 0, right: -theme.spacing.xs)
    modelButton.backgroundColor = theme.colors.fill
    modelButton.layer.cornerRadius = 15
    modelButton.contentEdgeInsets = UIEdgeInsets(top: 0, left: theme.spacing.md, bottom: 0, right: theme.spacing.md + (onPressModel == nil ? 0 : theme.spacing.xs))
    modelButton.accessibilityLabel = model.map(labels.model)
    modelButton.isEnabled = onPressModel != nil
    thinkingButton.isHidden = onThinkingChange == nil
    let thinkingTitle = theme.typography.footnote.with(.semibold).attributedString(labels.thinkingToggle, color: thinking ? theme.colors.accent : theme.colors.label)
    thinkingButton.setAttributedTitle(thinkingTitle, for: .normal)
    thinkingWidth.constant = pillWidth(thinkingTitle)
    thinkingButton.contentEdgeInsets = UIEdgeInsets(top: 0, left: theme.spacing.md, bottom: 0, right: theme.spacing.md)
    thinkingButton.layer.cornerRadius = 15
    thinkingButton.backgroundColor = thinking ? theme.colors.accentSoft : theme.colors.fill
    thinkingButton.accessibilityLabel = labels.thinkingToggleDescription
    thinkingButton.accessibilityTraits = thinking ? [.button, .selected] : .button
    dictate.isHidden = onPressDictate == nil
    dictate.accessibilityLabel = dictating ? labels.stopDictating : labels.dictate
    dictate.accessibilityTraits = dictating ? [.button, .selected] : .button
    updatePrimary()
  }

  /*
   * A pill is as wide as its title on whole pixels and its padding. A button alone rounds its
   * title up to a point.
   */
  private func pillWidth(_ title: NSAttributedString?) -> CGFloat {
    2 * theme.spacing.md + ShadowListUtilsPlatform.ceilToPixel(title?.size().width ?? 0)
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

  private static let modelChevronSize: CGFloat = 10
}
