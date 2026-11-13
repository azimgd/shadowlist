import UIKit

/*
 * The immutable transcript row snapshot. UI callbacks and expansion updates stay on the owner.
 */
struct ShadowListUtilsAssistantRow: ShadowListUtilsRow {
  let item: ShadowListUtilsAssistantMessage
  let turn: ShadowListUtilsAssistantTurn?
  let busy: Bool
  let isLatest: Bool
  let actionsVisible: Bool
  let thinkingExpanded: Bool
  let expandedToolKeys: Set<String>
  let theme: ShadowListUtilsTheme
  let labels: ShadowListUtilsAssistantLabels
  let handlers: ShadowListUtilsAssistantHandlers
  let onActionsChange: (String, Bool) -> Void
  let onExpansionChange: (String, Bool, Set<String>) -> Void
  let contentVersion: Int

  init(item: ShadowListUtilsAssistantMessage, turn: ShadowListUtilsAssistantTurn?, busy: Bool, isLatest: Bool,
       actionsVisible: Bool, thinkingExpanded: Bool, expandedToolKeys: Set<String>, theme: ShadowListUtilsTheme,
       labels: ShadowListUtilsAssistantLabels, handlers: ShadowListUtilsAssistantHandlers,
       onActionsChange: @escaping (String, Bool) -> Void,
       onExpansionChange: @escaping (String, Bool, Set<String>) -> Void) {
    self.item = item
    self.turn = turn
    self.busy = busy
    self.isLatest = isLatest
    self.actionsVisible = actionsVisible
    self.thinkingExpanded = thinkingExpanded
    self.expandedToolKeys = expandedToolKeys
    self.theme = theme
    self.handlers = handlers
    self.onActionsChange = onActionsChange
    self.onExpansionChange = onExpansionChange
    // Resolve application formatters now, before the list may prefetch layouts on another thread.
    var frozen = labels
    if case let .assistant(reply) = item {
      let current = ShadowListUtilsAssistantReplyMessage.resolvedTurn(message: reply, live: turn)
      let active = current.status == .streaming && (current.thinkingMs ?? 0) == 0
      let duration = ((current.thinkingMs ?? 0) / 1000).rounded()
      let seconds = !duration.isFinite || duration < 1 ? 1 : duration >= Double(Int.max) ? Int.max : Int(duration)
      let thought = current.thinking?.isEmpty == false && !active ? labels.thoughtFor(seconds) : ""
      let position = reply.variants.count > 1 ? labels.versionPosition(reply.variantIndex + 1, reply.variants.count) : ""
      var statuses: [ShadowListUtilsAssistantToolStatus: String] = [:]
      for call in current.toolCalls { statuses[call.status] = labels.toolStatus(call.status) }
      frozen.thoughtFor = { _ in thought }
      frozen.versionPosition = { _, _ in position }
      frozen.toolStatus = { statuses[$0] ?? "" }
    }
    self.labels = frozen
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(turn)
    hasher.combine(busy)
    hasher.combine(isLatest)
    hasher.combine(actionsVisible)
    hasher.combine(thinkingExpanded)
    hasher.combine(expandedToolKeys)
    hasher.combine(handlers.onFeedback != nil)
    hasher.combine(handlers.onRetry != nil)
    contentVersion = hasher.finalize()
  }

  var key: String { item.key }
  var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsAssistantRowView.self }

  func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    switch item {
    case let .user(prompt):
      let layout = ShadowListUtilsAssistantUserMessage.layout(message: prompt, width: width,
        actionsVisible: actionsVisible, theme: theme, labels: labels)
      return ShadowListUtilsAssistantRowLayout(width: width, height: layout.size.height, content: .user(layout))
    case let .assistant(reply):
      let layout = ShadowListUtilsAssistantReplyMessage.layout(message: reply, live: turn, width: width,
        busy: busy, isLatest: isLatest, thinkingExpanded: thinkingExpanded, expandedToolKeys: expandedToolKeys,
        hasFeedback: handlers.onFeedback != nil, hasRetry: handlers.onRetry != nil, theme: theme, labels: labels)
      return ShadowListUtilsAssistantRowLayout(width: width, height: layout.size.height, content: .assistant(layout))
    case .end:
      return ShadowListUtilsAssistantRowLayout(width: width, height: 1, content: .end)
    }
  }
}
