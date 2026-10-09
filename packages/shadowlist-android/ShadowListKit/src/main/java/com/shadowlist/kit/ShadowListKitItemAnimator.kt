package com.shadowlist.kit

/*
 * Animates rows for animatesChanges. The list works out which rows came, went and moved, places
 * them and calls these to animate. animateRemoval must call completion when it ends, which gives
 * the cell back to the reuse pool. animateMove gets a cell already at its new place, and fromX
 * and fromY are where it showed before, relative to that place.
 */
interface ShadowListKitItemAnimator {
  fun animateInsert(listView: ShadowListKitListView, cell: ShadowListKitListCell)
  fun animateRemoval(listView: ShadowListKitListView, cell: ShadowListKitListCell, completion: () -> Unit)
  fun animateMove(listView: ShadowListKitListView, cell: ShadowListKitListCell, fromX: Float, fromY: Float)
}

/*
 * The animator a list starts with: new rows fade in, removed rows fade out and rows that stay
 * slide, for durationMs.
 */
open class ShadowListKitDefaultItemAnimator : ShadowListKitItemAnimator {
  companion object {
    const val CHANGE_DURATION_MS = 250L
  }

  var durationMs = CHANGE_DURATION_MS

  override fun animateInsert(listView: ShadowListKitListView, cell: ShadowListKitListCell) {
    cell.animate().cancel()
    cell.translationX = 0f
    cell.translationY = 0f
    cell.alpha = 0f
    cell.animate().alpha(1f).setDuration(durationMs).start()
  }

  override fun animateRemoval(listView: ShadowListKitListView, cell: ShadowListKitListCell, completion: () -> Unit) {
    cell.animate().alpha(0f).setDuration(durationMs).withEndAction(completion).start()
  }

  override fun animateMove(listView: ShadowListKitListView, cell: ShadowListKitListCell, fromX: Float, fromY: Float) {
    if (fromX == 0f && fromY == 0f && cell.translationX == 0f && cell.translationY == 0f) return
    cell.animate().cancel()
    cell.translationX = fromX
    cell.translationY = fromY
    cell.animate().translationX(0f).translationY(0f).alpha(1f).setDuration(durationMs).start()
  }
}
