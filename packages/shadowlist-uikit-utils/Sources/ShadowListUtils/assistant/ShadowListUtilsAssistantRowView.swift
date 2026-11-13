import UIKit

/*
 * Applies the immutable row layout. Local disclosure changes report their state to the list.
 */
final class ShadowListUtilsAssistantRowView: ShadowListUtilsRowView {
  private var user: ShadowListUtilsAssistantUserMessage?
  private var reply: ShadowListUtilsAssistantReplyMessage?

  required init(reuseIdentifier: String?) { super.init(reuseIdentifier: reuseIdentifier) }

  override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsAssistantRow else { return }
    restingBackgroundColor = row.theme.colors.background
    isAccessibilityElement = false
    switch row.item {
    case let .user(prompt):
      reply?.isHidden = true
      let view: ShadowListUtilsAssistantUserMessage
      if let previous = user, previous.theme == row.theme { view = previous }
      else { user?.removeFromSuperview(); view = ShadowListUtilsAssistantUserMessage(message: prompt, theme: row.theme); user = view; addSubview(view) }
      view.isHidden = false
      view.message = prompt
      view.labels = row.labels
      view.busy = row.busy
      view.actionsVisible = row.actionsVisible
      view.onCopy = row.handlers.onCopy
      view.onEdit = row.handlers.onEdit
      view.onLayoutChange = { [weak self, weak view] in
        guard let current = self?.row as? ShadowListUtilsAssistantRow, let view else { return }
        current.onActionsChange(current.key, view.actionsVisible)
      }
    case let .assistant(message):
      user?.isHidden = true
      let view: ShadowListUtilsAssistantReplyMessage
      if let previous = reply, previous.theme == row.theme { view = previous }
      else { reply?.removeFromSuperview(); view = ShadowListUtilsAssistantReplyMessage(message: message, theme: row.theme); reply = view; addSubview(view) }
      view.isHidden = false
      view.message = message
      view.turn = row.turn
      view.labels = row.labels
      view.busy = row.busy
      view.isLatest = row.isLatest
      view.thinkingExpanded = row.thinkingExpanded
      view.expandedToolKeys = row.expandedToolKeys
      view.onCopy = row.handlers.onCopy
      view.onCopyCode = row.handlers.onCopyCode
      view.onShare = row.handlers.onShare
      view.onRegenerate = row.handlers.onRegenerate
      view.onRetry = row.handlers.onRetry
      view.onSelectVariant = row.handlers.onSelectVariant
      view.onFeedback = row.handlers.onFeedback
      view.onFollowUp = row.handlers.onFollowUp
      view.onOpenLink = row.handlers.onOpenLink
      view.onLayoutChange = { [weak self, weak view] in
        guard let current = self?.row as? ShadowListUtilsAssistantRow, let view else { return }
        current.onExpansionChange(current.key, view.thinkingExpanded, view.expandedToolKeys)
      }
    case .end:
      user?.isHidden = true
      reply?.isHidden = true
    }
  }

  override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let layout = layout as? ShadowListUtilsAssistantRowLayout else { return }
    switch layout.content {
    case let .user(prepared):
      user?.frame = CGRect(origin: .zero, size: prepared.size)
      user?.apply(prepared)
    case let .assistant(prepared):
      reply?.frame = CGRect(origin: .zero, size: prepared.size)
      reply?.apply(prepared)
    case .end: break
    }
  }
}
