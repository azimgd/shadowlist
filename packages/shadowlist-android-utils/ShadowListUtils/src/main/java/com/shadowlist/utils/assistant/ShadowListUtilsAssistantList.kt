package com.shadowlist.utils

import android.content.Context
import android.view.inputmethod.InputMethodManager

/*
 * A controlled transcript. The app owns messages, saved variants and live turns; the list only
 * asks for actions. End markers remain real rows and are excluded from anchoring by metadata.
 */
class ShadowListUtilsAssistantList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)
  private var storedItems = emptyList<ShadowListUtilsAssistantMessage>()
  private var storedTurns = emptyMap<String, ShadowListUtilsAssistantTurn>()
  private var actionsVisibleKeys = emptySet<String>()
  private var thinkingExpandedKeys = emptySet<String>()
  private var expandedToolKeys = emptyMap<String, Set<String>>()
  private var endKeys = emptySet<String>()

  var items: List<ShadowListUtilsAssistantMessage>
    get() = storedItems
    set(value) = setItems(value, false)
  fun setItems(items: List<ShadowListUtilsAssistantMessage>, animated: Boolean) {
    storedItems = items.map { it.snapshot() }
    val keys = items.map { it.key }.toSet()
    actionsVisibleKeys = actionsVisibleKeys.intersect(keys)
    thinkingExpandedKeys = thinkingExpandedKeys.intersect(keys)
    expandedToolKeys = expandedToolKeys.filterKeys { it in keys }
    val next = items.filterIsInstance<ShadowListUtilsAssistantMessage.End>().map { it.key }.toSet()
    nonAnchorKeys = (nonAnchorKeys - endKeys) + next
    endKeys = next
    rebuildRows(animated)
  }
  var turns: Map<String, ShadowListUtilsAssistantTurn>
    get() = storedTurns
    set(value) = setTurns(value, false)
  fun setTurns(turns: Map<String, ShadowListUtilsAssistantTurn>, animated: Boolean) { storedTurns = turns.mapValues { it.value.snapshot() }; rebuildRows(animated) }
  var streaming = false
    set(value) { field = value; reloadRows() }
  var rowForItem: ((ShadowListUtilsAssistantMessage) -> ShadowListUtilsRow)? = null
    set(value) { field = value; reloadRows() }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; reloadRows() }
  var onCopy: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onCopyCode: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onShare: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onRegenerate: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onRetry: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onSelectVariant: ((String, Int) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onFeedback: ((String, ShadowListUtilsAssistantFeedback?) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onFollowUp: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onOpenLink: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }
  var onEdit: ((String) -> Unit)? = null
    set(value) { field = value; reloadRows() }

  init { inverted = true; followAppends = true; allowsSelection = false }

  override fun makeRows(): List<ShadowListUtilsRow> {
    val latestKey = storedItems.getOrNull(storedItems.size - 2)?.key
    val handlers = ShadowListUtilsAssistantHandlers(onCopy, onCopyCode, onShare, onRegenerate, onRetry, onSelectVariant, onFeedback, onFollowUp, onOpenLink, onEdit)
    return storedItems.map { item -> rowForItem?.invoke(item) ?: ShadowListUtilsAssistantRow(item, storedTurns[item.key], streaming, item.key == latestKey,
      item.key in actionsVisibleKeys, item.key in thinkingExpandedKeys, expandedToolKeys[item.key].orEmpty(), theme, labels, handlers,
      onActionsChange = { key, visible ->
        actionsVisibleKeys = if (visible) actionsVisibleKeys + key else actionsVisibleKeys - key
        rebuildRows()
      }, onExpansionChange = { key, expanded, toolKeys ->
        thinkingExpandedKeys = if (expanded) thinkingExpandedKeys + key else thinkingExpandedKeys - key
        expandedToolKeys = expandedToolKeys + (key to toolKeys.toSet())
        rebuildRows()
      }) }
  }

  override fun makeAdapter(): ShadowListUtilsListAdapter = object : ShadowListUtilsListAdapter(this) {
    override fun willBeginDragging(listView: com.shadowlist.kit.ShadowListKitListView) {
      (context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager)?.hideSoftInputFromWindow(windowToken, 0)
    }
  }
}
