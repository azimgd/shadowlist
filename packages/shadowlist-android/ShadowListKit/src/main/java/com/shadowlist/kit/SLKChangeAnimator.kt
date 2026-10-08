package com.shadowlist.kit

import android.view.View

/*
 * The insert and delete animations of a list with animatesChanges. A data change records
 * where every mounted row is on screen. After the layout pass that applies it, rows that stay
 * slide from there to their new place, new rows fade in and removed rows fade out where they
 * were. The core anchors the content as usual: rows that hold still on screen do not move.
 */
internal class SLKChangeAnimator(private val list: SLKListView) {
  private var pending = false

  // Screen position of every mounted row before the change, left and top, by key.
  private val before = HashMap<String, FloatArray>()
  private val inserted = HashSet<String>()
  private val removed = HashSet<String>()

  /*
   * A change is about to reach the core. Several changes before one layout add up, and the
   * first one records the screen.
   */
  fun capture(removed: List<String>, inserted: List<String>) {
    if (!list.isLaidOut || list.hasHeldRow) return
    if (!pending) {
      recordScreen()
      this.removed.clear()
      this.inserted.clear()
      pending = true
    }
    val insertedNow = HashSet(inserted)
    val removedNow = HashSet(removed)
    // A key on both sides moved. It slides like any row that stays.
    for (key in removed) if (key !in insertedNow) this.removed.add(key) else this.inserted.remove(key)
    for (key in inserted) if (key !in removedNow) this.inserted.add(key) else this.removed.remove(key)
  }

  private fun recordScreen() {
    before.clear()
    for ((key, cell) in list.mounted) {
      if (cell.visibility != View.VISIBLE) continue
      before[key] = floatArrayOf(screenLeft(cell), screenTop(cell))
    }
  }

  // Where the cell shows now, a slide still running included.
  private fun screenLeft(cell: SLKListCell): Float = cell.left + cell.translationX - list.scrollX
  private fun screenTop(cell: SLKListCell): Float = cell.top + cell.translationY - list.scrollY

  /*
   * Keep a removed row's cell on screen where it was and fade it out. Returns false when the
   * cell should go back to the pool right away.
   */
  fun fadeOut(key: String, cell: SLKListCell): Boolean {
    if (!pending || key !in removed || cell.visibility != View.VISIBLE) return false
    val previous = before[key] ?: return false
    cell.animate().cancel()
    cell.translationX = previous[0] - (cell.left - list.scrollX)
    cell.translationY = previous[1] - (cell.top - list.scrollY)
    list.itemAnimator.animateRemoval(list, cell) { list.recycleCell(cell) }
    return true
  }

  /*
   * After the layout pass: slide the rows that stay and fade in the new ones.
   */
  fun run() {
    if (!pending) return
    pending = false
    var shiftX = 0f
    var shiftY = 0f
    for (cell in list.mounted.values.sortedBy { it.row }) {
      if (cell.visibility != View.VISIBLE || cell.row < 0) continue
      val key = list.keyAt(cell.row) ?: continue
      val previous = before[key]
      when {
        previous != null -> {
          shiftX = previous[0] - (cell.left - list.scrollX)
          shiftY = previous[1] - (cell.top - list.scrollY)
          list.itemAnimator.animateMove(list, cell, shiftX, shiftY)
        }
        key in inserted -> list.itemAnimator.animateInsert(list, cell)
        // Came into view without a place on screen before. It moves with the row above it.
        else -> list.itemAnimator.animateMove(list, cell, shiftX, shiftY)
      }
    }
    before.clear()
    inserted.clear()
    removed.clear()
  }
}
