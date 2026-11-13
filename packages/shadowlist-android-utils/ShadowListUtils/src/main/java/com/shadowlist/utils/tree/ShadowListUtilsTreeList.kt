package com.shadowlist.utils

import android.content.Context
import com.shadowlist.kit.ShadowListKitTreeRow
import com.shadowlist.kit.ShadowListKitTreeRows

/*
 * A file tree whose visible rows and expansion splices come from the shared TreeSource.
 * expandedKeys null owns expansion here; a set makes expansion controlled by the app.
 * A controlled press only requests onExpandedChange until the app supplies its next set.
 */
class ShadowListUtilsTreeList : ShadowListUtilsList {
  constructor(context: Context, initialExpandedKeys: Set<String> = emptySet()) : super(context) { ownExpandedKeys = initialExpandedKeys; source.setExpandedKeys(initialExpandedKeys) }
  constructor(context: Context, theme: ShadowListUtilsTheme, initialExpandedKeys: Set<String> = emptySet()) : super(context, theme) { ownExpandedKeys = initialExpandedKeys; source.setExpandedKeys(initialExpandedKeys) }

  private val source = ShadowListKitTreeRows()
  private val nodes = HashMap<String, ShadowListUtilsTreeNode>()
  private var storedItems = emptyList<ShadowListUtilsTreeNode>()
  private var ownExpandedKeys = emptySet<String>()
  private var pendingExpandedKeys: Set<String>? = null

  var items: List<ShadowListUtilsTreeNode>
    get() = storedItems
    set(value) = setItems(value, false)

  fun setItems(items: List<ShadowListUtilsTreeNode>, animated: Boolean) {
    pendingExpandedKeys = null
    val expansion = expandedKeys ?: ownExpandedKeys
    storedItems = items
    nodes.clear()
    val encoded = ArrayList<ShadowListKitTreeRow>()
    fun encode(items: List<ShadowListUtilsTreeNode>, depth: Int) {
      for (item in items) {
        require(!nodes.containsKey(item.key)) { "Tree keys must be unique" }
        nodes[item.key] = item
        encoded.add(ShadowListKitTreeRow(item.key, depth, !item.children.isNullOrEmpty(), item.key in expansion))
        item.children?.let { encode(it, depth + 1) }
      }
    }
    encode(items, 0)
    source.reset(encoded)
    source.setExpandedKeys(expansion)
    rebuildRows(animated)
  }

  var expandedKeys: Set<String>? = null
    set(value) {
      field = value
      pendingExpandedKeys = null
      source.setExpandedKeys(value ?: ownExpandedKeys)
      rebuildRows()
    }
  var onExpandedChange: ((Set<String>) -> Unit)? = null
  var onPressItem: ((ShadowListUtilsTreeNode) -> Unit)? = null
    set(value) { field = value; rebuildRows() }
  var rowForItem: ((ShadowListUtilsTreeListRenderItemInfo) -> ShadowListUtilsRow)? = null
    set(value) { field = value; reloadRows() }
  var indentWidth: Float = 16f
    set(value) { field = value; reloadRows() }
  var labels = ShadowListUtilsTreeLabels()
    set(value) { field = value; reloadRows() }

  init { allowsSelection = true }

  override fun makeRows(): List<ShadowListUtilsRow> {
    return source.rows.mapNotNull { row ->
      val item = nodes[row.key] ?: return@mapNotNull null
      val info = ShadowListUtilsTreeListRenderItemInfo(item, row.depth, indentWidth.coerceAtLeast(0f) * row.depth, row.expanded, row.hasChildren) { toggle(row.key) }
      rowForItem?.invoke(info)
        ?: ShadowListUtilsTreeRow(item, row.depth, row.hasChildren, row.expanded, indentWidth, row.hasChildren || onPressItem != null, theme, labels)
    }
  }

  fun scrollToNode(key: String, viewPosition: Double = 0.0) {
    val index = source.rows.indexOfFirst { it.key == key }
    if (index >= 0) scrollToItem(index, viewPosition, true)
  }

  fun toggle(key: String) {
    val before = source.expandedKeys.toSet()
    val index = source.rows.indexOfFirst { it.key == key }
    if (index < 0) return
    val row = source.rows[index]
    if (!row.hasChildren) return
    pendingExpandedKeys?.let(source::setExpandedKeys)
    source.toggleKey(key)
    val requested = source.expandedKeys.toSet()
    if (expandedKeys != null) {
      pendingExpandedKeys = requested
      source.setExpandedKeys(before)
    } else {
      ownExpandedKeys = requested
      rebuildRows()
    }
    onExpandedChange?.invoke(requested)
  }

  override fun didSelectItem(index: Int) {
    val row = source.rows.getOrNull(index) ?: return
    if (row.hasChildren) toggle(row.key) else nodes[row.key]?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }
}
