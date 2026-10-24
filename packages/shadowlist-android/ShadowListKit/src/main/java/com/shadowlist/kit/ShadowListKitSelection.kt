package com.shadowlist.kit

import android.view.ViewConfiguration

/*
 * Taps that select rows and the highlight under a resting finger. The selected keys are the
 * core's ListSelection in the list's ShadowListKitListState, which outlives the core the list
 * drops on detach. This shows its changes on the mounted cells and tells the delegate.
 */
internal class ShadowListKitSelection(private val list: ShadowListKitListView) {
  private val state get() = list.state
  private var highlightedCell: ShadowListKitListCell? = null
  private val highlightRunnable = Runnable { highlightPending() }
  private var highlightX = 0f
  private var highlightY = 0f

  val count: Int get() = state.selectedCount

  fun isSelected(key: String): Boolean = state.isSelected(key)

  fun setMultiple(multiple: Boolean) = state.setMultipleSelection(multiple)

  /*
   * The data changed. Rows whose key is gone drop out of the selection.
   */
  fun dropDeletedKeys(keys: List<String>) = state.retainSelection(keys)

  /*
   * A tap on a row, by the core's ListSelection.tap. Multiple selection toggles it, single
   * selection moves to it.
   */
  fun userSelected(cell: ShadowListKitListCell) {
    val index = cell.index
    val key = list.keys.getOrNull(cell.row) ?: return
    val tap = state.tap(key) { list.delegate?.shouldSelectItem(list, index) != false }
    when (tap.outcome) {
      ShadowListKitListState.TAP_TOGGLED_OFF -> {
        cell.setSelected(false, true)
        list.delegate?.didDeselectItem(list, index)
      }
      ShadowListKitListState.TAP_SELECTED -> {
        for (previous in tap.deselected) {
          list.mounted[previous]?.setSelected(false, true)
          val previousIndex = list.keys.indexOf(previous).let(list::itemForRow)
          if (previousIndex >= 0) list.delegate?.didDeselectItem(list, previousIndex)
        }
        if (!cell.isSelected) cell.setSelected(true, true)
        list.delegate?.didSelectItem(list, index)
      }
    }
  }

  val selectedIndices: IntArray
    get() = state.selectedRows(list.keys).map(list::itemForRow).filter { it >= 0 }.toIntArray()

  fun selectItem(index: Int, animated: Boolean) {
    val key = list.keys.getOrNull(list.rowForItem(index)) ?: return
    if (!list.allowsSelection) return
    for (previous in state.select(key)) list.mounted[previous]?.setSelected(false, animated)
    list.mounted[key]?.let { if (!it.isSelected) it.setSelected(true, animated) }
  }

  fun deselectItem(index: Int, animated: Boolean) {
    val key = list.keys.getOrNull(list.rowForItem(index)) ?: return
    if (state.deselect(key)) list.mounted[key]?.setSelected(false, animated)
  }

  fun clear() {
    for (key in state.clearSelection()) list.mounted[key]?.setSelected(false, false)
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
