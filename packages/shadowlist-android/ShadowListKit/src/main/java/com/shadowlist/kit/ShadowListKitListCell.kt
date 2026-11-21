package com.shadowlist.kit

import android.content.Context
import android.os.Build
import android.os.Bundle
import android.view.ViewPropertyAnimator
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import androidx.core.view.ViewCompat
import java.lang.ref.WeakReference

/*
 * The view that shows an item. Subclasses lay out their children by hand in onMeasure and onLayout for speed,
 * and report their height for an exact width with an unspecified height. Plain FrameLayout
 * children work too, they are only slower.
 *
 * The list calls setHighlighted while a finger rests on a selectable item, setSelected for
 * selected items and setEditing while the list is editing, only when the value changes. Override
 * them to show it. Highlighted also sets the pressed state and selected the selected state, which
 * state list drawables follow.
 */
open class ShadowListKitListCell(context: Context, val reuseIdentifier: String?) : FrameLayout(context) {
  private var swipeAccessibility: ShadowListKitSwipeAccessibility? = null
  private var boardActionIds = emptyList<Int>()
  private var boardActionLabels = emptyList<String>()
  private var boardActionTargets = emptyList<WeakReference<ShadowListKitListView>>()
  companion object {
    const val NO_INDEX = -1
  }

  /*
   * The item the cell shows, or NO_INDEX while it waits in the reuse pool and for a section header or footer.
   */
  var index: Int = NO_INDEX
    internal set

  /*
   * The row the core places for the cell, which differs from index in a list with sections.
   */
  internal var row: Int = NO_INDEX

  /*
   * The mount pass that last wanted this cell. Cells left behind go back to the pool.
   */
  internal var mountGeneration = 0L

  var highlighted = false
    private set

  /*
   * Something asked for the cell's animator since the list last cancelled it.
   */
  private var animated = false

  var editing = false
    private set

  override fun animate(): ViewPropertyAnimator {
    animated = true
    return super.animate()
  }

  /*
   * Stops what runs on the cell's animator. Without one asked for there is nothing to stop.
   */
  internal fun cancelAnimations() {
    if (!animated) return
    animated = false
    super.animate().cancel()
  }

  /*
   * A cell shown or hidden other than by the list draws again in a driven list's content.
   */
  override fun setVisibility(visibility: Int) {
    if (visibility != this.visibility) (parent as? ShadowListKitListView)?.contentStale = true
    super.setVisibility(visibility)
  }

  /*
   * Called before the cell is handed out again by dequeueReusableCell.
   */
  open fun prepareForReuse() { clearSwipeAccessibility() }

  internal fun clearSwipeAccessibility() { swipeAccessibility?.clear() }

  override fun onDetachedFromWindow() {
    clearSwipeAccessibility()
    super.onDetachedFromWindow()
  }

  open fun setHighlighted(highlighted: Boolean, animated: Boolean) {
    this.highlighted = highlighted
    isPressed = highlighted
  }

  open fun setSelected(selected: Boolean, animated: Boolean) {
    isSelected = selected
  }

  open fun setEditing(editing: Boolean, animated: Boolean) {
    this.editing = editing
  }

  /*
   * The row's place in the whole list, for accessibility services that read "item 12 of 1000".
   */
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    val list = parent as? ShadowListKitListView
    if (list == null) { clearSwipeAccessibility(); return }
    val actions = swipeAccessibility ?: ShadowListKitSwipeAccessibility(this) {
      val owner = parent as? ShadowListKitListView
      if (owner == null) null else owner.swipe to this
    }.also { swipeAccessibility = it }
    actions.refresh()
    actions.appendTo(info)
    refreshBoardActions(list)
    for (at in boardActionIds.indices) {
      info.addAction(AccessibilityNodeInfo.AccessibilityAction(boardActionIds[at], boardActionLabels[at]))
    }
    if (index < 0) {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) info.isHeading = true
      return
    }
    val columns = list.numberOfColumns
    val line = index / columns
    val track = index % columns
    info.collectionItemInfo = if (list.horizontal) {
      AccessibilityNodeInfo.CollectionItemInfo.obtain(track, 1, line, 1, false, isSelected)
    } else {
      AccessibilityNodeInfo.CollectionItemInfo.obtain(line, 1, track, 1, false, isSelected)
    }
  }

  /*
   * AndroidX allocates the custom action ids, which compose with a row's own actions. Refresh
   * only when membership or labels change, and read the current item again on invocation.
   */
  private fun refreshBoardActions(list: ShadowListKitListView) {
    val targets = if (index >= 0 && list.delegate?.canMoveItem(list, index) != false) list.dragBoard?.moveTargets(list).orEmpty() else emptyList()
    val labels = targets.map { context.getString(R.string.shadowlist_move_to, it.dragListLabel?.takeIf { label -> label.isNotEmpty() } ?: it.dragListId) }
    if (labels == boardActionLabels && targets.size == boardActionTargets.size && targets.indices.all { targets[it] === boardActionTargets[it].get() }) return
    boardActionIds.forEach { ViewCompat.removeAccessibilityAction(this, it) }
    val ids = ArrayList<Int>()
    val keptLabels = ArrayList<String>()
    val keptTargets = ArrayList<WeakReference<ShadowListKitListView>>()
    for (at in targets.indices) {
      val destination = WeakReference(targets[at])
      val id = ViewCompat.addAccessibilityAction(this, labels[at]) { _, _ ->
        val source = parent as? ShadowListKitListView
        val target = destination.get()
        source != null && target != null && index >= 0 && source.dragBoard?.performAccessibilityMove(source, index, target) == true
      }
      if (id != NO_ID) {
        ids.add(id)
        keptLabels.add(labels[at])
        keptTargets.add(destination)
      }
    }
    boardActionIds = ids
    boardActionLabels = keptLabels
    boardActionTargets = keptTargets
  }

  override fun performAccessibilityAction(action: Int, arguments: Bundle?): Boolean {
    val at = boardActionIds.indexOf(action)
    if (at >= 0) {
      val source = parent as? ShadowListKitListView ?: return false
      val destination = boardActionTargets[at].get() ?: return false
      return index >= 0 && source.dragBoard?.performAccessibilityMove(source, index, destination) == true
    }
    return super.performAccessibilityAction(action, arguments)
  }
}
