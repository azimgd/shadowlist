package com.shadowlist.kit

import android.view.ViewConfiguration

/*
 * Selected rows by key and the highlight under a resting finger. The same rules as the core's
 * ListSelection the UIKit list uses, kept here because the selection outlives the core, which
 * is dropped on detach.
 */
internal class ShadowListKitSelection(private val list: ShadowListKitListView) {
  private val selectedKeys = LinkedHashSet<String>()
  private var highlightedCell: ShadowListKitListCell? = null
  private val highlightRunnable = Runnable { highlightPending() }
  private var highlightX = 0f
  private var highlightY = 0f

  val count: Int get() = selectedKeys.size

  fun isSelected(key: String): Boolean = key in selectedKeys

  /*
   * The data changed. Rows whose key is gone drop out of the selection.
   */
  fun dropRemovedKeys(keys: List<String>) {
    if (selectedKeys.isNotEmpty()) selectedKeys.retainAll(HashSet(keys))
  }

  /*
   * A tap on a row. Multiple selection toggles it, single selection moves to it. The rules of
   * the core's ListSelection.tap, see selection_tap_selects_moves_and_toggles. The selection
   * stays here because every cell bind reads it.
   */
  fun userSelected(cell: ShadowListKitListCell) {
    val index = cell.index
    val key = list.keys.getOrNull(cell.row) ?: return
    if (list.allowsMultipleSelection && key in selectedKeys) {
      selectedKeys.remove(key)
      cell.setSelected(false, true)
      list.delegate?.didDeselectItem(list, index)
      return
    }
    if (list.delegate?.shouldSelectItem(list, index) == false) return
    if (!list.allowsMultipleSelection) {
      for (previous in selectedKeys.toList()) {
        if (previous == key) continue
        selectedKeys.remove(previous)
        list.mounted[previous]?.setSelected(false, true)
        val previousIndex = list.keys.indexOf(previous).let(list::itemForRow)
        if (previousIndex >= 0) list.delegate?.didDeselectItem(list, previousIndex)
      }
    }
    selectedKeys.add(key)
    if (!cell.isSelected) cell.setSelected(true, true)
    list.delegate?.didSelectItem(list, index)
  }

  val selectedIndices: IntArray
    get() {
      if (selectedKeys.isEmpty()) return IntArray(0)
      val items = ArrayList<Int>()
      for ((row, key) in list.keys.withIndex()) {
        if (key in selectedKeys) list.itemForRow(row).takeIf { it >= 0 }?.let(items::add)
      }
      return items.toIntArray()
    }

  fun selectItem(index: Int, animated: Boolean) {
    val key = list.keys.getOrNull(list.rowForItem(index)) ?: return
    if (!list.allowsSelection) return
    if (!list.allowsMultipleSelection) {
      for (previous in selectedKeys) if (previous != key) list.mounted[previous]?.setSelected(false, animated)
      selectedKeys.clear()
    }
    selectedKeys.add(key)
    list.mounted[key]?.let { if (!it.isSelected) it.setSelected(true, animated) }
  }

  fun deselectItem(index: Int, animated: Boolean) {
    val key = list.keys.getOrNull(list.rowForItem(index)) ?: return
    if (selectedKeys.remove(key)) list.mounted[key]?.setSelected(false, animated)
  }

  fun clear() {
    for (key in selectedKeys) list.mounted[key]?.setSelected(false, false)
    selectedKeys.clear()
  }

  /*
   * A finger resting on a selectable row highlights it after the tap timeout, unless it moves.
   */
  fun highlightDown(x: Float, y: Float) {
    cancelHighlight()
    if (!list.allowsSelection || list.hasHeldRow || list.swipe.isOpen || list.swipe.closingTouch) return
    highlightX = x
    highlightY = y
    list.postDelayed(highlightRunnable, ViewConfiguration.getTapTimeout().toLong())
  }

  private fun highlightPending() {
    val cell = list.itemCellAt(highlightX, highlightY) ?: return
    if (list.delegate?.shouldHighlightItem(list, cell.index) == false) return
    cell.setHighlighted(true, false)
    highlightedCell = cell
  }

  fun cancelHighlight() {
    list.removeCallbacks(highlightRunnable)
    highlightedCell?.let { if (it.highlighted) it.setHighlighted(false, true) }
    highlightedCell = null
  }
}
