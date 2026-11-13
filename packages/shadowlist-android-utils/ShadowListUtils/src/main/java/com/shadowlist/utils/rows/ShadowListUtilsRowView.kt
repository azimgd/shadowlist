package com.shadowlist.utils

import android.content.Context
import android.os.Bundle
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import androidx.core.graphics.ColorUtils
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView

/*
 * The cell that shows a row. Subclasses set the content in fill and the frames in apply, and
 * never measure anything themselves. It is a ShadowListKitListCell, a plain FrameLayout, which
 * any other list can host too.
 */
open class ShadowListUtilsRowView(context: Context, reuseIdentifier: String) :
  ShadowListKitListCell(context, reuseIdentifier) {
  var row: ShadowListUtilsRow? = null
    private set

  /*
   * The layout shown and the cross size it was made for: the width, or the height in a
   * horizontal list.
   */
  private var rowLayout: ShadowListUtilsRowLayout? = null
  private var layoutCrossPx = NO_CROSS

  /*
   * The background at rest and while the row is pressed. Without a pressed color the row does
   * not highlight.
   */
  internal var restingBackgroundColor: Int = 0
    set(value) {
      field = value
      updateBackground()
    }
  internal var pressedBackgroundColor: Int? = null
    set(value) {
      field = value
      updateBackground()
    }

  /*
   * Whether the pressed background shows. A subclass adds its own states, like a selected row
   * of a list that is editing.
   */
  internal open val showsPressedBackground: Boolean get() = highlighted

  /*
   * Show a row. With a layout the frames are taken as they are. Without one the layout is
   * computed on the UI thread the first time the view knows its size, like a self-sizing cell.
   */
  fun configure(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout?) =
    configure(row, layout, if (layout == null) NO_CROSS else ANY_CROSS)

  /*
   * The same from a list, which knows the cross size the layout was made for.
   */
  internal fun configure(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout?, crossPx: Int) {
    this.row = row
    rowLayout = layout
    layoutCrossPx = crossPx
    fill(row)
    requestLayout()
  }

  /*
   * Take a row that shows the same content under the same key, like one built again with a new
   * closure. Nothing is filled or laid out.
   */
  internal open fun replaceRow(row: ShadowListUtilsRow) {
    this.row = row
  }

  /*
   * Set the content, like colors and images, without frames.
   */
  open fun fill(row: ShadowListUtilsRow) {}

  /*
   * Set the frames from the layout.
   */
  open fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {}

  override fun prepareForReuse() {
    super.prepareForReuse()
    row = null
    rowLayout = null
    layoutCrossPx = NO_CROSS
  }

  /*
   * The layout's size along the scroll axis. A horizontal list asks with an unbounded width.
   */
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    var width = MeasureSpec.getSize(widthMeasureSpec)
    var height = MeasureSpec.getSize(heightMeasureSpec)
    if (inHorizontalList) {
      if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.EXACTLY) width = ensureLayout(height)?.widthPx ?: 0
    } else if (MeasureSpec.getMode(heightMeasureSpec) != MeasureSpec.EXACTLY) {
      height = ensureLayout(width)?.heightPx ?: 0
    }
    setMeasuredDimension(width, height)
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val row = row ?: return
    val layout = ensureLayout(if (inHorizontalList) bottom - top else right - left) ?: return
    apply(row, layout)
  }

  /*
   * The layout for a cross size: the one the view holds when it was made for it, otherwise a
   * new one.
   */
  private fun ensureLayout(crossPx: Int): ShadowListUtilsRowLayout? {
    val row = row ?: return null
    if (crossPx <= 0) return null
    rowLayout?.let { if (layoutCrossPx == crossPx || layoutCrossPx == ANY_CROSS) return it }
    layoutCrossPx = crossPx
    return row.layout(crossPx).also { rowLayout = it }
  }

  /*
   * The list the cell is in, or null while it waits in the reuse pool.
   */
  internal val enclosingList: ShadowListKitListView? get() = parent as? ShadowListKitListView

  private val inHorizontalList: Boolean get() = enclosingList?.horizontal == true

  /*
   * The row left the screen. A row with state of its own, like a nested list's position, saves
   * it.
   */
  internal open fun didEndDisplaying(list: ShadowListUtilsList) {}

  // region Highlight

  override fun setHighlighted(highlighted: Boolean, animated: Boolean) {
    super.setHighlighted(highlighted, animated)
    updateBackground()
  }

  /*
   * The pressed color shows the moment a press starts and leaves the moment it ends. A pressed
   * color with an alpha is laid over the resting one.
   */
  internal fun updateBackground() {
    val pressed = pressedBackgroundColor
    val shown = if (pressed != null && showsPressedBackground) ColorUtils.compositeColors(pressed, restingBackgroundColor) else restingBackgroundColor
    setBackgroundColor(shown)
  }

  // endregion

  // region Accessibility

  /*
   * A row of an item in a list that selects on a tap reads as a button, and has the actions
   * the row's list offers a screen reader for its item.
   */
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    val list = enclosingList ?: return
    if (index < 0) return
    if (list.allowsSelection) info.className = Button::class.java.name
    for (action in (list as? ShadowListUtilsList)?.accessibilityActions(index).orEmpty()) {
      info.addAction(AccessibilityNodeInfo.AccessibilityAction(action.id, action.label))
    }
  }

  override fun performAccessibilityAction(action: Int, arguments: Bundle?): Boolean {
    val list = enclosingList as? ShadowListUtilsList
    val custom = if (index < 0) null else list?.accessibilityActions(index)?.firstOrNull { it.id == action }
    return custom?.perform() ?: super.performAccessibilityAction(action, arguments)
  }

  // endregion

  // region Touches

  /*
   * Whether a part of the row with its own tap, like an image or a retry line, is under a point
   * in the row's coordinates. The list then leaves the tap to that part.
   */
  internal open fun claimsTouch(x: Float, y: Float): Boolean = false

  // endregion

  private companion object {
    /*
     * What layoutCrossPx holds without a layout, and with a layout the app handed over, which
     * is shown at any size.
     */
    const val NO_CROSS = -1
    const val ANY_CROSS = 0
  }
}

/*
 * Something a screen reader can do with an item besides pressing it. id is one of the library's
 * action ids, which names the action to the platform. perform returns whether it did anything.
 */
internal class ShadowListUtilsAccessibilityAction(val id: Int, val label: String, val perform: () -> Boolean)
