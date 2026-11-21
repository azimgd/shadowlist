package com.shadowlist.kit

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View
import android.view.ViewConfiguration
import android.view.animation.DecelerateInterpolator
import kotlin.math.abs

/*
 * Swipe actions. A move across the scroll axis on a row with actions moves the row and shows
 * its buttons behind it. The core's SwipeReveal in the host layer decides how far the row
 * follows and where it rests. A touch anywhere else closes an open row, and a drag from there
 * still scrolls.
 */
internal class ShadowListKitSwipeController(private val host: ShadowListKitSwipeHost) {
  constructor(list: ShadowListKitListView) : this(ShadowListKitListSwipeHost(list))

  private val view get() = host.view
  private fun cross(x: Float, y: Float): Float = if (host.horizontal) y else x
  private fun along(x: Float, y: Float): Float = if (host.horizontal) x else y
  private val touchSlop = ViewConfiguration.get(view.context).scaledTouchSlop
  private val reveal = ShadowListKitSwipeReveal()
  private var velocityTracker: VelocityTracker? = null
  private var animator: ValueAnimator? = null

  /*
   * The row swiped open and its buttons, or null.
   */
  var cell: ShadowListKitListCell? = null
    private set
  private var actionsView: ShadowListKitSwipeActionsView? = null
  private var offset = 0f
  private var startOffset = 0f

  /*
   * The row at offset is past the full swipe point.
   */
  private var pastFull = false

  private var downX = 0f
  private var downY = 0f
  private var tracking = false

  /*
   * This touch closed the open row. It selects nothing but may still scroll the list.
   */
  var closingTouch = false
    private set

  /*
   * This touch is on an action button and goes to it.
   */
  private var buttonTouch = false

  val isOpen: Boolean get() = cell != null

  fun isSwipedOut(candidate: ShadowListKitListCell): Boolean =
    candidate === cell && reveal.isSwipedOut(offset.toDouble())

  /*
   * Watch a touch for a swipe and own it once one runs. Returns whether the swipe took the event.
   */
  fun handle(event: MotionEvent): Boolean {
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> return down(event)
      MotionEvent.ACTION_MOVE -> return move(event)
      MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> return up(event)
    }
    return tracking || closingTouch
  }

  private fun down(event: MotionEvent): Boolean {
    tracking = false
    closingTouch = false
    buttonTouch = false
    downX = event.x
    downY = event.y
    velocityTracker?.clear()
    track(event)
    val open = cell ?: return false
    if (actionsView?.hasButtonAt(event.x + view.scrollX, event.y + view.scrollY) == true) {
      buttonTouch = true
      return false
    }
    if (host.itemCellAt(event.x, event.y) !== open) {
      close(true)
      closingTouch = true
      host.cancelHighlight()
    }
    return false
  }

  private fun move(event: MotionEvent): Boolean {
    if (closingTouch) return false
    if (buttonTouch) return false
    track(event)
    if (!tracking) {
      val crossDelta = cross(event.x, event.y) - cross(downX, downY)
      val alongDelta = along(event.x, event.y) - along(downX, downY)
      // A list already scrolling under the finger keeps the touch.
      if (host.isScrolling) return false
      if (abs(crossDelta) <= touchSlop || abs(crossDelta) <= abs(alongDelta) || !begin(crossDelta)) return false
      tracking = true
      host.beginSwipe()
      startOffset = offset
    }
    val translation = cross(event.x, event.y) - cross(downX, downY)
    val wasPast = pastFull
    if (cell == null) return true
    reveal.move(startOffset.toDouble(), translation.toDouble(), crossSize())
    applyReveal()
    if (pastFull != wasPast) cell?.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
    return true
  }

  private fun up(event: MotionEvent): Boolean {
    // The flag lasts until the next touch. The tap it would make is dropped on the way.
    if (closingTouch) return false
    if (tracking) {
      tracking = false
      // A cancelled swipe closes the row and never runs a full swipe's action.
      if (event.actionMasked == MotionEvent.ACTION_CANCEL) {
        close(true)
        return true
      }
      track(event)
      val tracker = velocityTracker
      tracker?.computeCurrentVelocity(1000)
      val velocity = if (tracker == null) 0f else if (host.horizontal) tracker.yVelocity else tracker.xVelocity
      settle(velocity)
      return true
    }
    // A tap on the open row closes it.
    val open = cell
    if (open != null && !buttonTouch && event.actionMasked == MotionEvent.ACTION_UP &&
      host.itemCellAt(event.x, event.y) === open) {
      close(true)
      return true
    }
    return false
  }

  private fun track(event: MotionEvent) {
    val tracker = velocityTracker ?: VelocityTracker.obtain().also { velocityTracker = it }
    tracker.addMovement(event)
  }

  private fun crossSize(): Double {
    val view = actionsView ?: return 0.0
    return (if (host.horizontal) view.height else view.width).toDouble()
  }

  /*
   * Start swiping the row under the touch's start toward a side that has actions.
   */
  private fun begin(crossDelta: Float): Boolean {
    if (!host.allowsSwipe) return false
    val target = host.itemCellAt(downX, downY) ?: return false
    val isCurrent = currentContent(target)
    if (!isCurrent()) return false
    if (target === cell) {
      stopAnimator()
      return true
    }
    val leading = actions(target, leadingSide = true)
    if (!isCurrent()) return false
    val trailing = actions(target, leadingSide = false)
    if (!isCurrent()) return false
    if ((if (crossDelta > 0) leading else trailing) == null) return false
    close(false)
    open(target, leading, trailing)
    return true
  }

  private fun actions(target: ShadowListKitListCell, leadingSide: Boolean): ShadowListKitSwipeActionsConfiguration? {
    val configuration = host.actions(target, leadingSide)
    return configuration?.takeIf { it.actions.isNotEmpty() }
  }

  /* Existing mounted identity survives no provider callback unless all of it still matches. */
  private fun currentContent(target: ShadowListKitListCell): () -> Boolean {
    val row = target.row
    val index = target.index
    val generation = target.mountGeneration
    val key = host.keyForCell(target)
    return {
      host.allowsSwipe && host.isValid(target) && target.row == row && target.index == index &&
        target.mountGeneration == generation && host.keyForCell(target) == key
    }
  }

  private fun open(target: ShadowListKitListCell, leading: ShadowListKitSwipeActionsConfiguration?, trailing: ShadowListKitSwipeActionsConfiguration?) {
    val view = ShadowListKitSwipeActionsView(view.context, leading, trailing, host.horizontal, host.density) { perform(it) }
    host.addActionsView(view)
    view.measure(
      View.MeasureSpec.makeMeasureSpec(target.width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(target.height, View.MeasureSpec.EXACTLY))
    view.layout(target.left, target.top, target.right, target.bottom)
    reveal.begin(view.leadingSizes, view.trailingSizes, leading?.performsFirstActionWithFullSwipe == true,
      trailing?.performsFirstActionWithFullSwipe == true, (if (host.horizontal) target.height else target.width).toDouble())
    cell = target
    actionsView = view
    offset = 0f
    pastFull = false
    if (target.highlighted) target.setHighlighted(false, false)
  }

  /*
   * Show the row at value, through the same core call as a touch move.
   */
  private fun apply(value: Float) {
    offset = value
    if (cell == null) return
    reveal.show(value.toDouble(), crossSize())
    applyReveal()
  }

  /*
   * Move the row and its buttons to what the core wrote last.
   */
  private fun applyReveal() {
    val values = reveal.out
    offset = values[ShadowListKitSwipeReveal.OUT_OFFSET].toFloat()
    pastFull = values[ShadowListKitSwipeReveal.OUT_PAST_FULL] != 0.0
    val target = cell ?: return
    if (host.horizontal) target.translationY = offset else target.translationX = offset
    actionsView?.layoutFor(offset, values)
  }

  private fun settle(velocity: Float) {
    reveal.settle(offset.toDouble(), velocity.toDouble(), ShadowListKitCore.SWIPE_FLING_VELOCITY_DP * host.density)
    val side = reveal.restSide
    val full = reveal.restFull
    animateTo(reveal.restOffset.toFloat())
    if (full) {
      val view = actionsView ?: return
      val configuration = if (side == ShadowListKitSwipeReveal.SIDE_LEADING) view.leading else view.trailing
      configuration?.actions?.firstOrNull()?.let(::perform)
    }
  }

  /*
   * Ease the row to target. An animation another one replaced, or a cancelled one, ends
   * nothing. ValueAnimator.cancel calls onAnimationEnd too, and a row grabbed again while it
   * closed would otherwise lose its swipe under the finger.
   */
  private fun animateTo(target: Float) {
    stopAnimator()
    val swiped = cell ?: return
    animator = ValueAnimator.ofFloat(offset, target).apply {
      duration = ShadowListKitCore.SWIPE_DURATION_MS
      interpolator = DecelerateInterpolator()
      addUpdateListener { if (cell === swiped) apply(it.animatedValue as Float) }
      addListener(object : AnimatorListenerAdapter() {
        override fun onAnimationEnd(animation: Animator) {
          if (animator !== animation) return
          animator = null
          if (cell === swiped && target == 0f) tearDown()
        }
      })
      start()
    }
  }

  private fun stopAnimator() {
    val running = animator ?: return
    animator = null
    running.cancel()
  }

  /*
   * Run an action. Its completion closes the row it was swiped open on, unless the action
   * deleted the row or the row closed and opened again since. A reused cell can show another
   * row by then, and the actions view of the swipe tells them apart.
   */
  private fun perform(action: ShadowListKitSwipeAction) {
    val view = actionsView ?: return
    action.handler(action) { _ ->
      view.post { if (actionsView === view) close(true) }
    }
  }

  /* Accessibility invokes the current configuration without first opening a visual swipe. */
  fun accessibilityActions(target: ShadowListKitListCell): List<ShadowListKitSwipeAction> {
    val isCurrent = currentContent(target)
    if (!isCurrent()) return emptyList()
    val leading = actions(target, true)?.actions.orEmpty()
    if (!isCurrent()) return emptyList()
    val trailing = actions(target, false)?.actions.orEmpty()
    if (!isCurrent()) return emptyList()
    return leading + trailing
  }

  fun performAccessibilityAction(target: ShadowListKitListCell, index: Int): Boolean {
    val isCurrent = currentContent(target)
    val action = accessibilityActions(target).getOrNull(index) ?: return false
    if (!isCurrent()) return false
    val session = actionsView
    action.handler(action) { _ ->
      view.post { if (cell === target && actionsView === session && host.isValid(target)) close(true) }
    }
    return true
  }

  fun close(animated: Boolean) {
    if (cell == null) return
    if (animated) {
      animateTo(0f)
      return
    }
    stopAnimator()
    apply(0f)
    tearDown()
  }

  private fun tearDown() {
    stopAnimator()
    actionsView?.let { host.removeActionsView(it) }
    actionsView = null
    cell?.let {
      if (host.horizontal) it.translationY = 0f else it.translationX = 0f
    }
    cell = null
    offset = 0f
    pastFull = false
  }

  /*
   * The list left its window. The open row closes, and the next touch starts fresh even if this
   * one never ended.
   */
  fun reset() {
    close(false)
    tracking = false
    closingTouch = false
    buttonTouch = false
  }

  /*
   * Give the velocity tracker back to its pool. The next touch obtains a new one.
   */
  fun releaseTracker() {
    velocityTracker?.recycle()
    velocityTracker = null
  }

  fun cellWillRecycle(recycled: ShadowListKitListCell) {
    if (recycled === cell) tearDown()
  }

  /*
   * Keep the buttons under the swiped row after a layout pass moved it. A row that left the
   * screen closes.
   */
  fun layout() {
    val swiped = cell ?: return
    val view = actionsView ?: return
    if (!host.isValid(swiped)) {
      tearDown()
      return
    }
    if (view.left != swiped.left || view.top != swiped.top || view.width != swiped.width || view.height != swiped.height) {
      view.measure(
        View.MeasureSpec.makeMeasureSpec(swiped.width, View.MeasureSpec.EXACTLY),
        View.MeasureSpec.makeMeasureSpec(swiped.height, View.MeasureSpec.EXACTLY))
      view.layout(swiped.left, swiped.top, swiped.right, swiped.bottom)
      reveal.show(offset.toDouble(), crossSize())
      view.layoutFor(offset, reveal.out)
    }
  }
}
