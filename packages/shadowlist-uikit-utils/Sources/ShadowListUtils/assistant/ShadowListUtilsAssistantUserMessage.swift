import ShadowListKit
import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * A controlled user message. A hold exposes Copy and Edit; editing stays application-owned.
 */
public final class ShadowListUtilsAssistantUserMessage: UIView {
  public var message: ShadowListUtilsAssistantPrompt { didSet { if message != oldValue { rebuild() } } }
  public var busy = false { didSet { edit.isEnabled = !busy } }
  public var onCopy: ((String) -> Void)?
  public var onEdit: ((String) -> Void)?
  public var labels = ShadowListUtilsAssistantLabels() { didSet { rebuild() } }
  public let theme: ShadowListUtilsTheme
  private let bubble = UIView()
  private let text = ShadowListKitTextView(frame: .zero)
  private let copy: ShadowListUtilsAssistantActionButton
  private let edit: ShadowListUtilsAssistantActionButton
  private var attachments: [ShadowListUtilsAssistantAttachmentChip] = []
  var actionsVisible = false
  var onLayoutChange: (() -> Void)?

  public init(message: ShadowListUtilsAssistantPrompt, theme: ShadowListUtilsTheme = .system) {
    self.message = message
    self.theme = theme
    copy = ShadowListUtilsAssistantActionButton(label: "Copy", icon: UIImage(systemName: "doc.on.doc"), showLabel: true, theme: theme)
    edit = ShadowListUtilsAssistantActionButton(label: "Edit", icon: UIImage(systemName: "pencil"), showLabel: true, theme: theme)
    super.init(frame: .zero)
    bubble.backgroundColor = theme.colors.elevated2
    bubble.layer.cornerRadius = theme.radius.xl
    bubble.isAccessibilityElement = true
    bubble.accessibilityTraits = .button
    let hold = UILongPressGestureRecognizer(target: self, action: #selector(held(_:)))
    hold.minimumPressDuration = 0.3
    bubble.addGestureRecognizer(hold)
    bubble.addSubview(text)
    [bubble, copy, edit].forEach(addSubview)
    copy.onPress = { [weak self] in
      guard let self else { return }
      self.onCopy?(self.message.text)
      self.hideActions()
    }
    edit.onPress = { [weak self] in
      guard let self, !self.busy else { return }
      self.onEdit?(self.message.key)
      self.hideActions()
    }
    rebuild()
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  private func rebuild() {
    attachments.forEach { $0.removeFromSuperview() }
    attachments = message.attachments.map { ShadowListUtilsAssistantAttachmentChip(attachment: $0, theme: theme) }
    attachments.forEach(addSubview)
    bubble.accessibilityLabel = message.text
    bubble.accessibilityHint = labels.showActionsHint
    bubble.accessibilityCustomActions = [UIAccessibilityCustomAction(name: labels.showActions) { [weak self] _ in self?.toggleActions(); return true }]
    copy.label = labels.copy
    edit.label = labels.edit
    setNeedsLayout()
    invalidateIntrinsicContentSize()
  }

  struct Layout {
    let size: CGSize
    let text: ShadowListKitTextLayout
    let bubbleFrame: CGRect
    let textFrame: CGRect
    let attachmentFrames: [CGRect]
    let copyFrame: CGRect
    let editFrame: CGRect
  }

  static func layout(message: ShadowListUtilsAssistantPrompt, width: CGFloat, actionsVisible: Bool, theme: ShadowListUtilsTheme,
                     labels: ShadowListUtilsAssistantLabels) -> Layout {
    let spacing = theme.spacing
    let metrics = ShadowListUtilsChatMetrics(
      spacingXxs: spacing.xxs, spacingXs: spacing.xs, spacingSm: spacing.sm, spacingMd: spacing.md, spacingLg: spacing.lg,
      bodyFontSize: theme.typography.body.fontSize, bodyLineHeight: theme.typography.body.scaledLineHeight(),
      bodyLetterSpacing: theme.typography.body.letterSpacing, captionLineHeight: theme.typography.caption.scaledLineHeight())
    var spec = ShadowListUtilsChatSizeSpec()
    _ = ShadowListUtilsCore.assistantUserMessageSizeSpec(&spec, hasText: true, hasAttachments: false, metrics: metrics)
    let textWidth = max(1, spec.widthFraction * width - spec.insetWidth)
    let line = theme.typography.body.layout(message.text, width: textWidth)
    let paddingX = SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL
    let paddingY = SHADOWLIST_UTILS_ASSISTANT_USER_BUBBLE_PADDING_VERTICAL
    let direction = ShadowListUtilsDirection(width: width)
    var y = spacing.sm
    var attachmentFrames: [CGRect] = []
    var lineWidth: CGFloat = 0
    var lineHeight: CGFloat = 0
    var pending: [CGRect] = []
    func finishLine() {
      let offset = width - spacing.lg - lineWidth
      attachmentFrames += pending.map { direction.place($0.offsetBy(dx: offset, dy: 0)) }
      pending = []
      y += lineHeight + spacing.sm
      lineWidth = 0
      lineHeight = 0
    }
    for attachment in message.attachments {
      let size = ShadowListUtilsAssistantAttachmentChip.layout(attachment: attachment, width: width - 2 * spacing.lg, removable: false, theme: theme).size
      if !pending.isEmpty, lineWidth + spacing.sm + size.width > width - 2 * spacing.lg { finishLine() }
      if !pending.isEmpty { lineWidth += spacing.sm }
      pending.append(CGRect(x: lineWidth, y: y, width: size.width, height: size.height))
      lineWidth += size.width
      lineHeight = max(lineHeight, size.height)
    }
    if !pending.isEmpty { finishLine() }
    let bubbleWidth = line.size.width + 2 * paddingX
    let bubble = message.text.isEmpty ? CGRect.zero : CGRect(
      x: width - spacing.lg - bubbleWidth, y: y, width: bubbleWidth, height: line.size.height + 2 * paddingY)
    if !message.text.isEmpty { y = bubble.maxY }
    let copyWidth = max(32, theme.typography.footnote.with(.semibold).layout(labels.copy, width: width).size.width + 38 + spacing.xs)
    let editWidth = max(32, theme.typography.footnote.with(.semibold).layout(labels.edit, width: width).size.width + 38 + spacing.xs)
    let actionsY = y + spacing.xs
    let editFrame = CGRect(x: width - spacing.lg - editWidth, y: actionsY, width: editWidth, height: 32)
    let copyFrame = CGRect(x: editFrame.minX - spacing.xs - copyWidth, y: actionsY, width: copyWidth, height: 32)
    if actionsVisible { y = actionsY + 32 }
    return Layout(size: CGSize(width: width, height: y + spacing.sm), text: line,
      bubbleFrame: direction.place(bubble), textFrame: CGRect(x: paddingX, y: paddingY, width: line.size.width, height: line.size.height),
      attachmentFrames: attachmentFrames, copyFrame: direction.place(copyFrame), editFrame: direction.place(editFrame))
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    Self.layout(message: message, width: size.width, actionsVisible: actionsVisible, theme: theme, labels: labels).size
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    apply(Self.layout(message: message, width: bounds.width, actionsVisible: actionsVisible, theme: theme, labels: labels))
  }

  func apply(_ layout: Layout) {
    bubble.frame = layout.bubbleFrame
    bubble.isHidden = message.text.isEmpty
    text.show(layout.text, theme.colors.label)
    text.frame = layout.textFrame
    for (index, attachment) in attachments.enumerated() { attachment.frame = layout.attachmentFrames[index] }
    copy.frame = layout.copyFrame
    edit.frame = layout.editFrame
    copy.isHidden = !actionsVisible
    edit.isHidden = !actionsVisible
  }

  @objc private func held(_ gesture: UILongPressGestureRecognizer) { if gesture.state == .began { toggleActions() } }
  func toggleActions() { actionsVisible.toggle(); changedLayout() }
  private func hideActions() { actionsVisible = false; changedLayout() }
  private func changedLayout() { setNeedsLayout(); invalidateIntrinsicContentSize(); onLayoutChange?() }
}
