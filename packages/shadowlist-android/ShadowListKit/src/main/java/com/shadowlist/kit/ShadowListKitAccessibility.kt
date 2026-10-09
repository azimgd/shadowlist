package com.shadowlist.kit

import android.os.Bundle
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import kotlin.math.roundToInt

/*
 * The list tells accessibility services how many rows it holds, not only the mounted ones,
 * and offers page scrolls and scrolling to any row. TalkBack scrolls forward when focus
 * leaves the last mounted row.
 */
internal class ShadowListKitAccessibility(private val list: ShadowListKitListView) {
  val className: CharSequence
    get() = if (list.numberOfColumns > 1) "android.widget.GridView" else "android.widget.ListView"

  fun initializeNodeInfo(info: AccessibilityNodeInfo) {
    val offset = list.offset
    val maxOffset = list.maxOffset
    val horizontal = list.horizontal
    val numberOfColumns = list.numberOfColumns
    info.isScrollable = maxOffset > 0
    if (offset > 0) {
      info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_BACKWARD)
      info.addAction(if (horizontal) AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_LEFT
        else AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP)
    }
    if (offset < maxOffset) {
      info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD)
      info.addAction(if (horizontal) AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_RIGHT
        else AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN)
    }
    info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_TO_POSITION)
    val lines = (list.itemCount + numberOfColumns - 1) / numberOfColumns
    val mode = if (list.allowsMultipleSelection) AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_MULTIPLE
      else if (list.allowsSelection) AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_SINGLE
      else AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_NONE
    info.collectionInfo = if (horizontal) {
      AccessibilityNodeInfo.CollectionInfo.obtain(numberOfColumns, lines, false, mode)
    } else {
      AccessibilityNodeInfo.CollectionInfo.obtain(lines, numberOfColumns, false, mode)
    }
  }

  fun initializeEvent(event: AccessibilityEvent) {
    event.isScrollable = list.maxOffset > 0
    event.itemCount = list.itemCount
    list.visibleRange?.let {
      event.fromIndex = it.first
      event.toIndex = it.last
    }
    if (list.horizontal) {
      event.scrollX = list.offset
      event.maxScrollX = list.maxOffset
    } else {
      event.scrollY = list.offset
      event.maxScrollY = list.maxOffset
    }
  }

  /*
   * Runs a scroll action. Returns null for an action the list leaves to View.
   */
  fun performAction(action: Int, arguments: Bundle?): Boolean? {
    val forward = if (list.horizontal) android.R.id.accessibilityActionScrollRight else android.R.id.accessibilityActionScrollDown
    val backward = if (list.horizontal) android.R.id.accessibilityActionScrollLeft else android.R.id.accessibilityActionScrollUp
    return when (action) {
      AccessibilityNodeInfo.ACTION_SCROLL_FORWARD, forward -> scrollByPage(1)
      AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD, backward -> scrollByPage(-1)
      android.R.id.accessibilityActionScrollToPosition -> scrollToPosition(arguments)
      else -> null
    }
  }

  /*
   * One viewport toward the end, or toward the start for a negative direction.
   */
  private fun scrollByPage(direction: Int): Boolean {
    val target = ShadowListKitCore.pageScrollTarget(list.offset.toDouble(), list.windowAlong.toDouble(), list.maxOffset.toDouble(), direction).roundToInt()
    if (target == list.offset) return false
    list.gesture.stop()
    list.writeOffset(target, byUser = true)
    list.layoutPass()
    list.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED)
    return true
  }

  private fun scrollToPosition(arguments: Bundle?): Boolean {
    val row = arguments?.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_ROW_INT, -1) ?: -1
    val column = arguments?.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_COLUMN_INT, 0) ?: 0
    val index = if (list.horizontal) column * list.numberOfColumns + row else row * list.numberOfColumns + column
    if (index !in 0 until list.itemCount) return false
    list.scrollToItem(index)
    list.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED)
    return true
  }
}
