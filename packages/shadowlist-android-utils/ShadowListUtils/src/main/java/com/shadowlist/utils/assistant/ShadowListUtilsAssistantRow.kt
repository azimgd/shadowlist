package com.shadowlist.utils

import android.content.Context

/*
 * An immutable row snapshot. Its layout uses model data and captured presentation state only;
 * application formatters resolve on the UI owner before this row reaches background prefetch.
 */
internal class ShadowListUtilsAssistantRow(
  item: ShadowListUtilsAssistantMessage,
  turn: ShadowListUtilsAssistantTurn?,
  val busy: Boolean,
  val isLatest: Boolean,
  val actionsVisible: Boolean,
  val thinkingExpanded: Boolean,
  expandedToolKeys: Set<String>,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsAssistantLabels,
  val handlers: ShadowListUtilsAssistantHandlers,
  val onActionsChange: (String, Boolean) -> Unit,
  val onExpansionChange: (String, Boolean, Set<String>) -> Unit,
) : ShadowListUtilsRow {
  val item = item.snapshot()
  val turn = turn?.snapshot()
  val expandedToolKeys = expandedToolKeys.toSet()
  private val texts = (item as? ShadowListUtilsAssistantMessage.Assistant)?.let { ShadowListUtilsAssistantReplyMessage.textSnapshot(it.reply, turn, labels) }
  override val key: String get() = item.key
  override val contentVersion = listOf(item, turn, busy, isLatest, actionsVisible, thinkingExpanded, this.expandedToolKeys, handlers.onFeedback != null, handlers.onRetry != null).hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsAssistantRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout = when (item) {
    is ShadowListUtilsAssistantMessage.User -> {
      val layout = ShadowListUtilsAssistantUserMessage.layout(item.prompt, widthPx, actionsVisible, theme, labels)
      ShadowListUtilsAssistantRowLayout(widthPx, layout.heightPx, ShadowListUtilsAssistantRowLayout.Content.User(layout))
    }
    is ShadowListUtilsAssistantMessage.Assistant -> {
      val layout = ShadowListUtilsAssistantReplyMessage.layout(item.reply, turn, widthPx, isLatest, thinkingExpanded, expandedToolKeys,
        handlers.onFeedback != null, handlers.onRetry != null, theme, labels, requireNotNull(texts))
      ShadowListUtilsAssistantRowLayout(widthPx, layout.heightPx, ShadowListUtilsAssistantRowLayout.Content.Assistant(layout))
    }
    is ShadowListUtilsAssistantMessage.End -> ShadowListUtilsAssistantRowLayout(widthPx, 1f.px, ShadowListUtilsAssistantRowLayout.Content.End)
  }

  companion object { const val REUSE_IDENTIFIER = "ShadowListUtilsAssistantRow" }
}
