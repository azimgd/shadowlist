package com.shadowlist.kit

import android.view.Choreographer
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.ViewConfiguration
import kotlin.math.abs

/*
 * Touch and hold to reorder. The held row follows the finger, the other rows slide aside to
 * open the gap, and the list scrolls while the row is held near an edge. The drop slot and
 * the shifts come from the core's ListDriver, which runs the host layer's DragReorder.
 */
internal class SLKDragController(private val list: SLKListView) : Choreographer.FrameCallback {
  companion object {
    private const val LIFT_ELEVATION_DP = 8f
  }

  private val longPressTimeout = ViewConfiguration.getLongPressTimeout().toLong()
  private val touchSlop = ViewConfiguration.get(list.context).scaledTouchSlop
  private val offsetOut = DoubleArray(2)

  /*
   * Where each shifted cell is headed, to animate only real changes.
   */
  private val shiftTargets = HashMap<SLKListCell, Long>()

  var heldCell: SLKListCell? = null
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
    Choreographer.getInstance().postFrameCallback(this)
    update()
    return true
  }

  private fun movableCellAt(x: Float, y: Float): SLKListCell? {
    val cell = list.itemCellAt(x, y) ?: return null
    if (list.delegate?.canMoveItem(list, cell.index) == false) return null
    return cell
  }

  private fun lift(cell: SLKListCell) {
    cell.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    cell.animate().scaleX(SLKCore.LIFT_SCALE).scaleY(SLKCore.LIFT_SCALE).translationZ(LIFT_ELEVATION_DP * list.density)
      .setDuration(SLKCore.LIFT_DURATION_MS).start()
    list.invalidate()
  }

  // endregion

  // region Follow

  /*
   * Place the held row under the finger and slide the others to open the gap.
   */
  fun update() {
    val cell = heldCell ?: return
    val core = list.core
    val held = core.heldIndex
    if (held < 0) {
      end(false)
      return
    }
    val along = list.contentAlongAt(touchX, touchY).toDouble()
    core.placeHeld(held, along, list.contentCrossAt(touchX, touchY).toDouble(), offsetOut)
    translate(cell, offsetOut[0].toFloat(), offsetOut[1].toFloat())
    val insertion = core.dragInsertionIndex
    core.dragUpdateInsertion(list.mountedIndices())
    applyShifts(core.dragInsertionIndex != insertion)
  }

  /*
   * Slide every mounted row except the held one by its shift. Rows mounted since the last
   * shift jump straight to theirs.
   */
  fun applyShifts(animated: Boolean) {
    for (cell in list.mounted.values) {
      if (cell === heldCell || cell.row < 0) continue
      list.core.dragShiftFor(cell.row, offsetOut)
      val along = offsetOut[0].toFloat()
      val cross = offsetOut[1].toFloat()
      val target = packTarget(along, cross)
      val known = shiftTargets[cell]
      if (known == target) continue
      shiftTargets[cell] = target
      if (animated && known != null) animateTranslation(cell, along, cross, SLKCore.SHIFT_DURATION_MS)
      else setTranslation(cell, along, cross)
    }
  }

  private fun packTarget(along: Float, cross: Float): Long =
    (along.toRawBits().toLong() shl 32) or (cross.toRawBits().toLong() and 0xffffffffL)

  private fun setTranslation(cell: SLKListCell, along: Float, cross: Float) {
    cell.animate().cancel()
    translate(cell, along, cross)
  }

  private fun translate(cell: SLKListCell, along: Float, cross: Float) {
    cell.translationX = if (list.horizontal) along else cross
    cell.translationY = if (list.horizontal) cross else along
  }

  private fun animateTranslation(cell: SLKListCell, along: Float, cross: Float, duration: Long) {
    cell.animate()
      .translationX(if (list.horizontal) along else cross)
      .translationY(if (list.horizontal) cross else along)
      .setDuration(duration).start()
  }

  /*
   * Scroll while the row is held near an edge. The move counts as the user's scrolling, the
   * same as a finger moving the list.
   */
  override fun doFrame(frameTimeNanos: Long) {
    if (!hasHeldRow) return
    val touch = list.windowAlongAt(touchX, touchY).toDouble()
    val offset = list.offset.toDouble()
    val next = SLKCore.dragAutoScrollOffset(
      touch, list.windowAlong.toDouble(), offset, list.maxOffset.toDouble(), list.density.toDouble())
    if (abs(next - offset) >= 1) {
      list.writeOffset(next.toInt(), byUser = true)
      update()
    }
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

  private fun drop(cell: SLKListCell) {
    for (mounted in list.mounted.values) {
      if (mounted !== cell) animateTranslation(mounted, 0f, 0f, SLKCore.DROP_DURATION_MS)
    }
    cell.animate().translationX(0f).translationY(0f).scaleX(1f).scaleY(1f).translationZ(0f)
      .setDuration(SLKCore.DROP_DURATION_MS).start()
  }

  // endregion
}
