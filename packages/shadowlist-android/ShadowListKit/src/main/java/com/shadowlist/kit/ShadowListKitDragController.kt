package com.shadowlist.kit

import android.view.Choreographer
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import kotlin.math.abs

/*
 * Touch and hold to reorder. The held row follows the finger, the other rows slide aside to
 * open the gap, and the list scrolls while the row is held near an edge. The drop slot and
 * the shifts come from the core's ListDriver, which runs the host layer's DragReorder.
 */
internal class ShadowListKitDragController(private val list: ShadowListKitListView) : Choreographer.FrameCallback {
  companion object {
    private const val LIFT_ELEVATION_DP = 8f
  }

  private val longPressTimeout = ViewConfiguration.getLongPressTimeout().toLong()
  private val touchSlop = ViewConfiguration.get(list.context).scaledTouchSlop

  /*
   * The mounted cells a drag frame shifts, their rows and what the core wrote for them, reused
   * from frame to frame.
   */
  private val frameCells = ArrayList<ShadowListKitListCell>()
  private var frameRows = IntArray(64)
  private var frameOut = DoubleArray(ShadowListKitCore.DRAG_FRAME_SLOTS + 128)

  /*
   * Where each shifted cell is headed, to animate only real changes.
   */
  private val shiftTargets = HashMap<ShadowListKitListCell, Long>()

  var heldCell: ShadowListKitListCell? = null
    private set

  val hasHeldRow: Boolean get() = heldCell != null

  /*
   * Where the held row is in the data now, or -1 without a drag.
   */
  val heldIndex: Int get() = if (hasHeldRow) list.core.heldIndex else -1

  /*
   * The finger in the list's own coordinates.
   */
  private var touchX = 0f
  private var touchY = 0f
  private var downX = 0f
  private var downY = 0f
  private var pressPending = false
  // The previous auto scroll frame's time, 0 before the first frame of a drag.
  private var lastFrameNanos = 0L

  /*
   * The finger moved since the row lifted. A row let go in place shows its menu instead.
   */
  private var movedSinceLift = false

  /*
   * A menu showed for this touch, which owns the rest of it.
   */
  private var menuShown = false

  /*
   * A hold lifts a row that can move. On a row that cannot it shows the row's menu.
   */
  private val longPress = Runnable {
    pressPending = false
    if (list.beginDragAt(touchX, touchY)) {
      movedSinceLift = false
      list.cancelChildTouches()
      return@Runnable
    }
    val cell = list.itemCellAt(touchX, touchY) ?: return@Runnable
    if (list.showMenu(cell)) {
      menuShown = true
      list.abandonScrollGesture()
      list.cancelChildTouches()
    }
  }

  /*
   * Watch a touch for a long press and own it once a drag runs or a menu shows. Returns
   * whether the drag took the event.
   */
  fun handle(event: MotionEvent): Boolean {
    if (!list.reorderEnabled && !list.hasMenus) return false
    touchX = event.x
    touchY = event.y
    if (menuShown) {
      if (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_CANCEL) menuShown = false
      return true
    }
    if (hasHeldRow) {
      followTouch(event)
      return true
    }
    watchLongPress(event)
    return false
  }

  fun cancelPress() = cancelLongPress()

  /*
   * A cell going back to the pool loses its shift. Reused for another row it must take that
   * row's shift even when the target matches the previous one.
   */
  fun cellWillRecycle(cell: ShadowListKitListCell) {
    shiftTargets.remove(cell)
  }

  private fun watchLongPress(event: MotionEvent) {
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        downX = event.x
        downY = event.y
        pressPending = true
        list.postDelayed(longPress, longPressTimeout)
      }
      MotionEvent.ACTION_MOVE -> {
        if (abs(event.x - downX) > touchSlop || abs(event.y - downY) > touchSlop) cancelLongPress()
      }
      MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> cancelLongPress()
    }
  }

  private fun cancelLongPress() {
    if (!pressPending) return
    pressPending = false
    list.removeCallbacks(longPress)
  }

  private fun followTouch(event: MotionEvent) {
    when (event.actionMasked) {
      MotionEvent.ACTION_MOVE -> {
        if (abs(event.x - downX) > touchSlop || abs(event.y - downY) > touchSlop) movedSinceLift = true
        update()
      }
      MotionEvent.ACTION_UP -> end(true)
      MotionEvent.ACTION_CANCEL -> end(false)
    }
  }

  // region Pick up

  /*
   * Pick up the row under a point. Returns false when there is none or it cannot move.
   */
  fun begin(x: Float, y: Float): Boolean {
    val cell = movableCellAt(x, y) ?: return false
    list.closeSwipeActions(true)
    touchX = x
    touchY = y
    list.core.dragBegin(cell.row, list.contentAlongAt(x, y).toDouble(), list.contentCrossAt(x, y).toDouble())
    heldCell = cell
    lift(cell)
    lastFrameNanos = 0L
    Choreographer.getInstance().postFrameCallback(this)
    update()
    return true
  }

  private fun movableCellAt(x: Float, y: Float): ShadowListKitListCell? {
    val cell = list.itemCellAt(x, y) ?: return null
    if (list.delegate?.canMoveItem(list, cell.index) == false) return null
    return cell
  }

  private fun lift(cell: ShadowListKitListCell) {
    cell.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    cell.animate().scaleX(ShadowListKitCore.LIFT_SCALE).scaleY(ShadowListKitCore.LIFT_SCALE).translationZ(LIFT_ELEVATION_DP * list.density)
      .setDuration(ShadowListKitCore.LIFT_DURATION_MS).start()
    list.invalidate()
  }

  // endregion

  // region Follow

  /*
   * Place the held row under the finger and slide the others to open the gap, in one core call.
   * Rows mounted since the last frame jump straight to their shift.
   */
  fun update() {
    val cell = heldCell ?: return
    frameCells.clear()
    for (mounted in list.mounted.values) {
      if (mounted !== cell && mounted.visibility == View.VISIBLE && mounted.row >= 0) frameCells.add(mounted)
    }
    val count = frameCells.size
    if (frameRows.size < count) frameRows = IntArray(count * 2)
    if (frameOut.size < ShadowListKitCore.DRAG_FRAME_SLOTS + count * 2) {
      frameOut = DoubleArray(ShadowListKitCore.DRAG_FRAME_SLOTS + count * 4)
    }
    for (at in 0 until count) frameRows[at] = frameCells[at].row
    val along = list.contentAlongAt(touchX, touchY).toDouble()
    val held = list.core.dragFrame(along, list.contentCrossAt(touchX, touchY).toDouble(), frameRows, count, frameOut)
    if (held < 0) {
      end(false)
      return
    }
    translate(cell, frameOut[ShadowListKitCore.DRAG_FRAME_HELD_ALONG].toFloat(), frameOut[ShadowListKitCore.DRAG_FRAME_HELD_CROSS].toFloat())
    val animated = frameOut[ShadowListKitCore.DRAG_FRAME_INSERTION_MOVED] != 0.0
    for (at in 0 until count) {
      val shifted = frameCells[at]
      val shiftAlong = frameOut[ShadowListKitCore.DRAG_FRAME_SLOTS + at * 2].toFloat()
      val shiftCross = frameOut[ShadowListKitCore.DRAG_FRAME_SLOTS + at * 2 + 1].toFloat()
      val target = packTarget(shiftAlong, shiftCross)
      val known = shiftTargets[shifted]
      if (known == target) continue
      shiftTargets[shifted] = target
      if (animated && known != null) animateTranslation(shifted, shiftAlong, shiftCross, ShadowListKitCore.SHIFT_DURATION_MS)
      else setTranslation(shifted, shiftAlong, shiftCross)
    }
    frameCells.clear()
  }

  private fun packTarget(along: Float, cross: Float): Long =
    (along.toRawBits().toLong() shl 32) or (cross.toRawBits().toLong() and 0xffffffffL)

  private fun setTranslation(cell: ShadowListKitListCell, along: Float, cross: Float) {
    cell.animate().cancel()
    translate(cell, along, cross)
  }

  private fun translate(cell: ShadowListKitListCell, along: Float, cross: Float) {
    cell.translationX = if (list.horizontal) along else cross
    cell.translationY = if (list.horizontal) cross else along
  }

  private fun animateTranslation(cell: ShadowListKitListCell, along: Float, cross: Float, durationMs: Long) {
    cell.animate()
      .translationX(if (list.horizontal) along else cross)
      .translationY(if (list.horizontal) cross else along)
      .setDuration(durationMs).start()
  }

  /*
   * Scroll while the row is held near an edge. The move counts as the user's scrolling, the
   * same as a finger moving the list.
   */
  override fun doFrame(frameTimeNanos: Long) {
    if (!hasHeldRow) return
    // The speed is per second. The first frame of a drag counts as one at 60 Hz.
    val dtSeconds = if (lastFrameNanos == 0L) 1.0 / 60.0 else (frameTimeNanos - lastFrameNanos) / 1e9
    lastFrameNanos = frameTimeNanos
    val touch = list.windowAlongAt(touchX, touchY).toDouble()
    val offset = list.offset.toDouble()
    val next = ShadowListKitCore.dragAutoScrollOffset(
      touch, list.windowAlong.toDouble(), offset, list.maxOffset.toDouble(), list.density.toDouble(), dtSeconds)
    // The offset write runs a layout pass, which runs the drag frame.
    if (abs(next - offset) >= 1) list.writeOffset(next.toInt(), byUser = true)
    Choreographer.getInstance().postFrameCallback(this)
  }

  // endregion

  // region Drop

  /*
   * Let go of the row. With commit the data moves, and the row flies from where it was let
   * go into its new slot.
   */
  fun end(commit: Boolean) {
    cancelLongPress()
    val cell = heldCell ?: return
    Choreographer.getInstance().removeFrameCallback(this)
    val from = list.core.dragOriginIndex
    val to = list.core.dragInsertionIndex
    heldCell = null
    list.core.dragEnd()
    shiftTargets.clear()
    val shownLeft = cell.left + cell.translationX
    val shownTop = cell.top + cell.translationY
    var moved = false
    if (commit && from >= 0 && to >= 0 && from != to) {
      for (mounted in list.mounted.values) setTranslation(mounted, 0f, 0f)
      moved = list.commitMove(from, to)
      cell.translationX = shownLeft - cell.left
      cell.translationY = shownTop - cell.top
    }
    drop(cell)
    // A row lifted and let go in place shows its menu.
    if (commit && !moved && !movedSinceLift) list.showMenu(cell)
  }

  private fun drop(cell: ShadowListKitListCell) {
    for (mounted in list.mounted.values) {
      if (mounted !== cell) animateTranslation(mounted, 0f, 0f, ShadowListKitCore.DROP_DURATION_MS)
    }
    cell.animate().translationX(0f).translationY(0f).scaleX(1f).scaleY(1f).translationZ(0f)
      .setDuration(ShadowListKitCore.DROP_DURATION_MS).start()
  }

  // endregion
}
