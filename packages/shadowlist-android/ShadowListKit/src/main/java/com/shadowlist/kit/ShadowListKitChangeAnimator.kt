package com.shadowlist.kit

import android.view.View

/*
 * The insert and delete animations of a list with animatesChanges. A data change records
 * where every mounted row is on screen. After the layout pass that applies it, rows that stay
 * slide from there to their new place, new rows fade in and removed rows fade out where they
 * were. The core anchors the content as usual: rows that hold still on screen do not move.
 * The core's ChangeAnimation decides what each row does. It is called once per change, never
 * on a scroll frame.
 */
internal class ShadowListKitChangeAnimator(private val list: ShadowListKitListView) {
  /*
   * A change was captured and waits for the layout pass. Saves the core call on other passes.
   */
  private var pending = false
  private val removedOut = DoubleArray(2)

  /*
   * A change is about to reach the core. Several changes before one layout add up, and the
   * first one records the screen.
   */
  fun capture(removed: List<String>, inserted: List<String>) {
    if (!list.isLaidOut || list.hasHeldRow) return
    val core = list.liveCore ?: return
    val keys = ArrayList<String>(list.mounted.size)
    val positions = DoubleArray(list.mounted.size * 2)
    for ((key, cell) in list.mounted) {
      if (cell.visibility != View.VISIBLE) continue
      positions[keys.size * 2] = screenLeft(cell).toDouble()
      positions[keys.size * 2 + 1] = screenTop(cell).toDouble()
      keys.add(key)
    }
    core.captureChange(removed, inserted, keys, positions.copyOf(keys.size * 2))
    pending = true
  }

  /*
   * Where the cell shows now, a slide still running included.
   */
  private fun screenLeft(cell: ShadowListKitListCell): Float = cell.left + cell.translationX - list.scrollX
  private fun screenTop(cell: ShadowListKitListCell): Float = cell.top + cell.translationY - list.scrollY

  /*
   * Keep a removed row's cell on screen where it was and fade it out. Returns false when the
   * cell should go back to the pool right away.
   */
  fun fadeOut(key: String, cell: ShadowListKitListCell): Boolean {
    if (!pending || cell.visibility != View.VISIBLE) return false
    val core = list.liveCore ?: return false
    if (!core.removedPosition(key, removedOut)) return false
    cell.animate().cancel()
    cell.translationX = removedOut[0].toFloat() - (cell.left - list.scrollX)
    cell.translationY = removedOut[1].toFloat() - (cell.top - list.scrollY)
    list.itemAnimator.animateRemoval(list, cell) { list.recycleCell(cell) }
    return true
  }

  /*
   * After the layout pass: slide the rows that stay and fade in the new ones.
   */
  fun run() {
    if (!pending) return
    pending = false
    val core = list.liveCore ?: return
    val cells = ArrayList<ShadowListKitListCell>(list.mounted.size)
    val keys = ArrayList<String>(list.mounted.size)
    for (cell in list.mounted.values.sortedBy { it.row }) {
      if (cell.visibility != View.VISIBLE || cell.row < 0) continue
      keys.add(list.keyAt(cell.row) ?: continue)
      cells.add(cell)
    }
    val positions = DoubleArray(cells.size * 2)
    for ((at, cell) in cells.withIndex()) {
      positions[at * 2] = (cell.left - list.scrollX).toDouble()
      positions[at * 2 + 1] = (cell.top - list.scrollY).toDouble()
    }
    val steps = core.runChange(keys, positions) ?: return
    for ((at, cell) in cells.withIndex()) {
      val step = at * ShadowListKitCore.CHANGE_STEP_SLOTS
      if (step + ShadowListKitCore.CHANGE_STEP_SLOTS > steps.size) break
      if (steps[step + ShadowListKitCore.CHANGE_STEP_KIND].toInt() == ShadowListKitCore.CHANGE_INSERT) {
        list.itemAnimator.animateInsert(list, cell)
      } else {
        val fromX = steps[step + ShadowListKitCore.CHANGE_STEP_FROM_X].toFloat()
        val fromY = steps[step + ShadowListKitCore.CHANGE_STEP_FROM_Y].toFloat()
        list.itemAnimator.animateMove(list, cell, fromX, fromY)
      }
    }
  }
}
