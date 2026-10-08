package com.shadowlist.kit

import android.content.Context
import android.os.Build
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout

/*
 * A row view. Subclasses lay out their children by hand in onMeasure and onLayout for speed,
 * and report their height for an exact width with an unspecified height. Plain FrameLayout
 * children work too, they are only slower.
 *
 * The list calls setHighlighted while a finger rests on a selectable row, setSelected for
 * selected rows and setEditing while the list is editing, only when the value changes. Override
 * them to show it. Highlighted also sets the pressed state and selected the selected state, which
 * state list drawables follow.
 */
open class SLKListCell(context: Context, val reuseIdentifier: String?) : FrameLayout(context) {
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

  var editing = false
    private set

  /*
   * Called before the cell is handed out again by dequeueReusableCell.
   */
  open fun prepareForReuse() {}

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
    val list = parent as? SLKListView ?: return
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
}
