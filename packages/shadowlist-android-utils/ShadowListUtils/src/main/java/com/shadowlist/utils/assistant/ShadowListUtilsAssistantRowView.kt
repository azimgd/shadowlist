package com.shadowlist.utils

import android.content.Context
import android.graphics.Color
import android.graphics.Rect
import android.view.View

internal class ShadowListUtilsAssistantRowView(context: Context) : ShadowListUtilsRowView(context, ShadowListUtilsAssistantRow.REUSE_IDENTIFIER) {
  private var content: View? = null
  private var contentTheme: ShadowListUtilsTheme? = null

  override fun fill(row: ShadowListUtilsRow) {
    val assistant = row as ShadowListUtilsAssistantRow
    restingBackgroundColor = Color.TRANSPARENT
    pressedBackgroundColor = null
    if (contentTheme != assistant.theme) { content?.let(::removeView); content = null; contentTheme = assistant.theme }
    when (val item = assistant.item) {
      is ShadowListUtilsAssistantMessage.User -> {
        val view = content as? ShadowListUtilsAssistantUserMessage ?: ShadowListUtilsAssistantUserMessage(context, item.prompt, assistant.theme).also(::replace)
        view.message = item.prompt
        view.actionsVisible = assistant.actionsVisible
        view.busy = assistant.busy
        view.labels = assistant.labels
        view.onCopy = assistant.handlers.onCopy
        view.onEdit = assistant.handlers.onEdit
        view.onLayoutChange = { assistant.onActionsChange(assistant.key, view.actionsVisible) }
      }
      is ShadowListUtilsAssistantMessage.Assistant -> {
        val view = content as? ShadowListUtilsAssistantReplyMessage ?: ShadowListUtilsAssistantReplyMessage(context, item.reply, assistant.turn, assistant.theme).also(::replace)
        view.message = item.reply
        view.turn = assistant.turn
        view.busy = assistant.busy
        view.isLatest = assistant.isLatest
        view.thinkingExpanded = assistant.thinkingExpanded
        view.expandedToolKeys = assistant.expandedToolKeys
        view.labels = assistant.labels
        view.onCopy = assistant.handlers.onCopy
        view.onCopyCode = assistant.handlers.onCopyCode
        view.onShare = assistant.handlers.onShare
        view.onRegenerate = assistant.handlers.onRegenerate
        view.onRetry = assistant.handlers.onRetry
        view.onSelectVariant = assistant.handlers.onSelectVariant
        view.onFeedback = assistant.handlers.onFeedback
        view.onFollowUp = assistant.handlers.onFollowUp
        view.onOpenLink = assistant.handlers.onOpenLink
        view.onLayoutChange = { assistant.onExpansionChange(assistant.key, view.thinkingExpanded, view.expandedToolKeys) }
      }
      is ShadowListUtilsAssistantMessage.End -> { content?.let(::removeView); content = null }
    }
  }

  private fun replace(view: View) { content?.let(::removeView); content = view; addView(view) }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val frames = layout as ShadowListUtilsAssistantRowLayout
    val view = content ?: return
    view.place(Rect(0, 0, frames.widthPx, frames.heightPx))
    when (val body = frames.content) {
      is ShadowListUtilsAssistantRowLayout.Content.User -> (view as ShadowListUtilsAssistantUserMessage).apply(body.layout)
      is ShadowListUtilsAssistantRowLayout.Content.Assistant -> (view as ShadowListUtilsAssistantReplyMessage).apply(body.layout)
      ShadowListUtilsAssistantRowLayout.Content.End -> Unit
    }
  }
  override fun prepareForReuse() {
    (content as? ShadowListUtilsAssistantUserMessage)?.onLayoutChange = null
    (content as? ShadowListUtilsAssistantReplyMessage)?.onLayoutChange = null
    super.prepareForReuse()
  }
}
