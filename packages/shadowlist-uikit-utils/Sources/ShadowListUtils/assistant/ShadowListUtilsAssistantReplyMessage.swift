import ShadowListKit
import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * A controlled reply. A live turn overrides the selected saved variant without changing either.
 */
public final class ShadowListUtilsAssistantReplyMessage: UIView {
  public var message: ShadowListUtilsAssistantReply { didSet { invalidateMessage(previousKey: oldValue.key) } }
  public var turn: ShadowListUtilsAssistantTurn? { didSet { changedLayout() } }
  public var busy = false { didSet { changedLayout() } }
  public var isLatest = false { didSet { changedLayout() } }
  public var onCopy: ((String) -> Void)?
  public var onCopyCode: ((String) -> Void)?
  public var onShare: ((String) -> Void)?
  public var onRegenerate: ((String) -> Void)?
  public var onRetry: ((String) -> Void)? { didSet { changedLayout() } }
  public var onSelectVariant: ((String, Int) -> Void)?
  public var onFeedback: ((String, ShadowListUtilsAssistantFeedback?) -> Void)? { didSet { changedLayout() } }
  public var onFollowUp: ((String) -> Void)?
  public var onOpenLink: ((String) -> Void)?
  public var labels = ShadowListUtilsAssistantLabels() { didSet { changedLayout() } }
  public let theme: ShadowListUtilsTheme
  var thinkingExpanded = false
  var expandedToolKeys: Set<String> = []
  var onLayoutChange: (() -> Void)?
  private let header = ShadowListKitTextView(frame: .zero)
  private var markdownViews: [String: ShadowListUtilsAssistantMarkdown] = [:]
  private var sectionViews: [UIView] = []
  private var actionViews: [UIView] = []
  private var copied = false
  private var resetCopied: DispatchWorkItem?

  public init(message: ShadowListUtilsAssistantReply, turn: ShadowListUtilsAssistantTurn? = nil, theme: ShadowListUtilsTheme = .system) {
    self.message = message
    self.turn = turn
    self.theme = theme
    super.init(frame: .zero)
    header.isAccessibilityElement = true
    header.accessibilityTraits = .header
    addSubview(header)
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  deinit { resetCopied?.cancel() }

  static func resolvedTurn(message: ShadowListUtilsAssistantReply, live: ShadowListUtilsAssistantTurn?) -> ShadowListUtilsAssistantTurn {
    live ?? (message.variants.indices.contains(message.variantIndex) ? message.variants[message.variantIndex] : ShadowListUtilsAssistantTurn(status: .done()))
  }

  enum Section {
    case typing(frame: CGRect)
    case text(key: String, layout: ShadowListKitTextLayout, frame: CGRect, error: Bool)
    case thinking(title: String, text: String, body: ShadowListUtilsAssistantDisclosureTextLayout, frame: CGRect, expanded: Bool)
    case tool(call: ShadowListUtilsAssistantToolCall, title: String, text: String, body: ShadowListUtilsAssistantDisclosureTextLayout, frame: CGRect, expanded: Bool)
    case markdown(key: String, text: String, streaming: Bool, frame: CGRect)
    case source(index: Int, source: ShadowListUtilsAssistantSource, frame: CGRect)
    case followUp(prompt: String, frame: CGRect)
    case retry(frame: CGRect)
  }

  enum Action: Hashable {
    case copy, share, regenerate, good, bad, previous, next, position
  }

  struct Layout {
    let size: CGSize
    let header: ShadowListKitTextLayout
    let headerFrame: CGRect
    let sections: [Section]
    let actions: [(Action, String, CGRect)]
    let streaming: Bool
  }

  static func layout(
    message: ShadowListUtilsAssistantReply, live: ShadowListUtilsAssistantTurn?, width: CGFloat,
    busy: Bool, isLatest: Bool, thinkingExpanded: Bool, expandedToolKeys: Set<String>,
    hasFeedback: Bool, hasRetry: Bool, theme: ShadowListUtilsTheme, labels: ShadowListUtilsAssistantLabels
  ) -> Layout {
    let turn = resolvedTurn(message: message, live: live)
    let streaming = turn.status == .streaming
    let spacing = theme.spacing
    let x = spacing.lg
    let inner = max(1, width - 2 * x)
    let direction = ShadowListUtilsDirection(width: width)
    let header = theme.typography.footnote.with(.semibold).layout(
      [labels.assistantName, message.model].compactMap { $0 }.joined(separator: " · "), width: inner)
    let headerFrame = CGRect(CGPoint(x: x, y: spacing.md), header)
    var y = headerFrame.maxY + spacing.md
    var sections: [Section] = []
    if let thinking = turn.thinking, !thinking.isEmpty {
      let active = streaming && (turn.thinkingMs ?? 0) == 0
      let title = ShadowListUtilsAssistantThinking.title(thinkingMs: turn.thinkingMs ?? 0, active: active, labels: labels)
      let layout = ShadowListUtilsAssistantThinking.layout(thinking: thinking, title: title, expanded: thinkingExpanded, width: inner, theme: theme)
      let preview = layout.text
      let body = layout.body
      let height = layout.size.height
      sections.append(.thinking(title: title, text: preview, body: body, frame: direction.place(CGRect(x: x, y: y, width: inner, height: height)), expanded: thinkingExpanded))
      y += height + spacing.md
    }
    if streaming, turn.thinking?.isEmpty != false, turn.content.isEmpty, turn.toolCalls.isEmpty {
      let height = 7 + 2 * spacing.sm
      sections.append(.typing(frame: direction.place(CGRect(x: x, y: y, width: inner, height: height))))
      y += height + spacing.md
    }
    let steps = ShadowListUtilsCore.stepsOf(turn.content, positions: turn.toolCalls.map { NSNumber(value: $0.at ?? 0) })
    for (index, step) in steps.enumerated() {
      if let text = step as? ShadowListUtilsAssistantTextStep {
        let blocks = ShadowListUtilsCore.parseMarkdown(text.text)
        let isStreaming = streaming && index == steps.count - 1
        var height: CGFloat = 0
        for block in blocks {
          height += ShadowListUtilsAssistantMarkdownBlock.layout(block: block, width: inner, theme: theme).height + spacing.md
        }
        if isStreaming { height += 6 + spacing.md }
        height = max(0, height - spacing.md)
        sections.append(.markdown(key: "text-\(text.at)", text: text.text, streaming: isStreaming,
          frame: direction.place(CGRect(x: x, y: y, width: inner, height: height))))
        y += height + spacing.md
      } else if let calls = step as? ShadowListUtilsAssistantCallsStep {
        for callIndex in calls.callIndices.map(\.intValue) where turn.toolCalls.indices.contains(callIndex) {
          let call = turn.toolCalls[callIndex]
          let expanded = expandedToolKeys.contains(call.key)
          let status = labels.toolStatus(call.status)
          let title = "\(call.name), \(status)"
          let layout = ShadowListUtilsAssistantToolCallCard.layout(call: call, title: title, status: status, labels: labels, expanded: expanded, width: inner, theme: theme)
          let bodyText = layout.text
          let body = layout.body
          let height = layout.size.height
          sections.append(.tool(call: call, title: title, text: bodyText, body: body,
            frame: direction.place(CGRect(x: x, y: y, width: inner, height: height)), expanded: expanded))
          y += height + spacing.xs
        }
        y += spacing.sm
      }
    }
    switch turn.status {
    case .stopped:
      let text = theme.typography.footnote.layout(labels.responseStopped, width: inner)
      sections.append(.text(key: "stopped", layout: text, frame: direction.place(CGRect(CGPoint(x: x, y: y), text)), error: false))
      y += text.size.height + spacing.md
    case let .failed(error):
      let text = theme.typography.footnote.layout(error.flatMap { $0.isEmpty ? nil : $0 } ?? labels.responseFailed, width: inner)
      sections.append(.text(key: "failed", layout: text, frame: direction.place(CGRect(CGPoint(x: x, y: y), text)), error: true))
      y += text.size.height + spacing.sm
      if hasRetry { sections.append(.retry(frame: direction.place(CGRect(x: x, y: y, width: inner, height: 32)))); y += 32 + spacing.md }
    case let .done(sources, _):
      for (index, source) in sources.enumerated() {
        let line = theme.typography.footnote.layout(source.title, width: inner - 20)
        let height = max(32, line.size.height + 12)
        sections.append(.source(index: index, source: source, frame: direction.place(CGRect(x: x, y: y, width: inner, height: height))))
        y += height + spacing.xs
      }
    default: break
    }
    var actions: [(Action, String)] = []
    if !turn.content.isEmpty { actions.append((.copy, labels.copy)) }
    actions.append((.regenerate, labels.regenerate))
    if hasFeedback { actions += [(.good, labels.goodResponse), (.bad, labels.badResponse)] }
    if !turn.content.isEmpty { actions.append((.share, labels.share)) }
    if message.variants.count > 1 { actions += [(.previous, labels.previousVersion), (.position, labels.versionPosition(message.variantIndex + 1, message.variants.count)), (.next, labels.nextVersion)] }
    var actionFrames: [(Action, String, CGRect)] = []
    var actionX = x
    for (action, label) in actions {
      let actionWidth = action == .position ? max(32, theme.typography.caption.layout(label, width: inner).size.width + 8) : 32
      actionFrames.append((action, label, direction.place(CGRect(x: actionX, y: y, width: actionWidth, height: 32))))
      actionX += actionWidth + spacing.xs
    }
    y += 32 + spacing.md
    if isLatest, case let .done(_, followUps) = turn.status {
      for prompt in followUps {
        let line = theme.typography.footnote.layout(prompt, width: inner - 24)
        let height = max(32, line.size.height + 16)
        sections.append(.followUp(prompt: prompt, frame: direction.place(CGRect(x: x, y: y, width: inner, height: height))))
        y += height + spacing.sm
      }
    }
    return Layout(size: CGSize(width: width, height: y + spacing.md), header: header,
      headerFrame: direction.place(headerFrame), sections: sections, actions: actionFrames, streaming: streaming)
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize { makeLayout(width: size.width).size }
  public override func layoutSubviews() { super.layoutSubviews(); apply(makeLayout(width: bounds.width)) }

  private func makeLayout(width: CGFloat) -> Layout {
    Self.layout(message: message, live: turn, width: width, busy: busy, isLatest: isLatest,
      thinkingExpanded: thinkingExpanded, expandedToolKeys: expandedToolKeys, hasFeedback: onFeedback != nil,
      hasRetry: onRetry != nil, theme: theme, labels: labels)
  }

  func apply(_ layout: Layout) {
    header.show(layout.header, theme.colors.secondaryLabel)
    header.frame = layout.headerFrame
    header.accessibilityLabel = [labels.assistantName, message.model].compactMap { $0 }.joined(separator: ", ")
    sectionViews.forEach { $0.removeFromSuperview() }
    sectionViews = []
    var usedMarkdown: Set<String> = []
    for section in layout.sections {
      switch section {
      case let .typing(frame):
        let view = ShadowListUtilsAssistantTypingIndicator(theme: theme)
        view.labels = labels; view.frame = frame
        addSection(view)
      case let .markdown(key, text, streaming, frame):
        let view = markdownViews[key] ?? ShadowListUtilsAssistantMarkdown(theme: theme)
        view.text = text
        view.streaming = streaming
        view.onCopyCode = { [weak self] in self?.onCopyCode?($0) }
        view.onOpenLink = onOpenLink
        markdownViews[key] = view
        usedMarkdown.insert(key)
        if view.superview == nil { addSubview(view) }
        view.frame = frame
      case let .text(key, text, frame, error):
        let view = ShadowListKitTextView(frame: frame)
        view.show(text, error ? theme.colors.red : theme.colors.secondaryLabel)
        view.isAccessibilityElement = true
        // The parser owns text; notices use their localized value from this layout's row.
        view.accessibilityLabel = key == "typing" ? labels.typing : error ? (resolvedTurn.status.errorText ?? labels.responseFailed) : labels.responseStopped
        addSection(view)
      case let .thinking(title, text, body, frame, expanded):
        let view = disclosure(title: title, text: text, body: body, frame: frame, expanded: expanded) { [weak self] in
          guard let self else { return }; self.thinkingExpanded.toggle(); self.changedLayout(); self.onLayoutChange?()
        }
        addSection(view)
      case let .tool(call, title, text, body, frame, expanded):
        let view = disclosure(title: title, text: text, body: body, frame: frame, expanded: expanded) { [weak self] in
          guard let self else { return }
          if self.expandedToolKeys.contains(call.key) { self.expandedToolKeys.remove(call.key) } else { self.expandedToolKeys.insert(call.key) }
          self.changedLayout(); self.onLayoutChange?()
        }
        (view as? ShadowListUtilsAssistantDisclosure)?.status = call.status
        view.backgroundColor = theme.colors.elevated
        view.layer.cornerRadius = theme.radius.md
        addSection(view)
      case let .source(index, source, frame):
        let button = textButton(source.title, frame: frame) { [weak self] in self?.open(source.url) }
        button.accessibilityLabel = labels.source(index + 1, source.title)
        addSection(button)
      case let .followUp(prompt, frame): addSection(textButton(prompt, frame: frame) { [weak self] in self?.onFollowUp?(prompt) })
      case let .retry(frame):
        let button = textButton(labels.retry, frame: frame) { [weak self] in guard let self, !self.busy else { return }; self.onRetry?(self.message.key) }
        button.isEnabled = !busy
        addSection(button)
      }
    }
    for (key, view) in markdownViews where !usedMarkdown.contains(key) { view.removeFromSuperview(); markdownViews.removeValue(forKey: key) }
    actionViews.forEach { $0.removeFromSuperview() }
    actionViews = []
    for (action, label, frame) in layout.actions {
      if action == .position {
        let view = UILabel(frame: frame)
        view.text = label
        view.textColor = theme.colors.secondaryLabel
        view.font = .systemFont(ofSize: theme.typography.caption.fontSize * ShadowListUtilsPlatform.textScale)
        view.isHidden = layout.streaming
        actionViews.append(view); addSubview(view)
      } else {
        let button = ShadowListUtilsAssistantActionButton(label: action == .copy && copied ? labels.copied : label,
          icon: UIImage(systemName: symbol(action)), selected: action == .good ? message.feedback == .good : action == .bad && message.feedback == .bad,
          theme: theme, onPress: { [weak self] in self?.perform(action) })
        button.frame = frame
        button.isHidden = layout.streaming
        button.accessibilityElementsHidden = layout.streaming
        button.isEnabled = !(busy && action == .regenerate) && !(action == .previous && message.variantIndex == 0)
          && !(action == .next && message.variantIndex == message.variants.count - 1)
        actionViews.append(button); addSubview(button)
      }
    }
  }

  private var resolvedTurn: ShadowListUtilsAssistantTurn { Self.resolvedTurn(message: message, live: turn) }

  private func addSection(_ view: UIView) { sectionViews.append(view); addSubview(view) }
  private func disclosure(title: String, text: String, body: ShadowListUtilsAssistantDisclosureTextLayout, frame: CGRect, expanded: Bool, onPress: @escaping () -> Void) -> UIView {
    let view = ShadowListUtilsAssistantDisclosure(theme: theme)
    view.frame = frame
    view.onPress = onPress
    view.show(title: title, text: text, layout: body, expanded: expanded)
    return view
  }

  private func textButton(_ title: String, frame: CGRect, onPress: @escaping () -> Void) -> UIButton {
    let button = UIButton(type: .system)
    button.frame = frame
    button.setTitle(title, for: .normal)
    button.titleLabel?.numberOfLines = 0
    button.titleLabel?.font = .systemFont(ofSize: theme.typography.footnote.fontSize * ShadowListUtilsPlatform.textScale)
    button.contentHorizontalAlignment = .leading
    button.addAction(UIAction { _ in onPress() }, for: .touchUpInside)
    return button
  }

  private func symbol(_ action: Action) -> String {
    switch action {
    case .copy: return copied ? "checkmark" : "doc.on.doc"
    case .share: return "square.and.arrow.up"
    case .regenerate: return "arrow.clockwise"
    case .good: return "hand.thumbsup"
    case .bad: return "hand.thumbsdown"
    case .previous: return "chevron.left"
    case .next: return "chevron.right"
    case .position: return ""
    }
  }

  func perform(_ action: Action) {
    let current = resolvedTurn
    guard current.status != .streaming else { return }
    switch action {
    case .copy:
      onCopy?(current.content)
      copied = true
      resetCopied?.cancel()
      let reset = DispatchWorkItem { [weak self] in self?.copied = false; self?.setNeedsLayout() }
      resetCopied = reset
      DispatchQueue.main.asyncAfter(deadline: .now() + 1.5, execute: reset)
      setNeedsLayout()
    case .share: onShare?(current.content)
    case .regenerate: if !busy { onRegenerate?(message.key) }
    case .good: onFeedback?(message.key, message.feedback == .good ? nil : .good)
    case .bad: onFeedback?(message.key, message.feedback == .bad ? nil : .bad)
    case .previous: if message.variantIndex > 0 { onSelectVariant?(message.key, message.variantIndex - 1) }
    case .next: if message.variantIndex + 1 < message.variants.count { onSelectVariant?(message.key, message.variantIndex + 1) }
    case .position: break
    }
  }

  private func open(_ href: String) {
    if let onOpenLink { onOpenLink(href) }
    else if ShadowListUtilsCore.isSafeUrl(href), let url = URL(string: ShadowListUtilsCore.trim(href)) { UIApplication.shared.open(url) }
  }
  private func invalidateMessage(previousKey: String) {
    if previousKey != message.key { thinkingExpanded = false; expandedToolKeys = []; copied = false; resetCopied?.cancel() }
    changedLayout()
  }
  private func changedLayout() { setNeedsLayout(); invalidateIntrinsicContentSize() }
  public override func didMoveToWindow() {
    super.didMoveToWindow()
    if window == nil { resetCopied?.cancel(); copied = false }
  }
}

private extension ShadowListUtilsAssistantTurnStatus {
  var errorText: String? { if case let .failed(error) = self { return error }; return nil }
}
