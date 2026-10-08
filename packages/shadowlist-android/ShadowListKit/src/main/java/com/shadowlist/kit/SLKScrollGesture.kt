package com.shadowlist.kit

import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.ViewConfiguration
import android.widget.OverScroller
import androidx.core.view.ViewCompat
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/*
 * Touch scrolling, flings and animated scrolls of one list. Reports the gesture phase the
 * core needs: tracking while a finger moves the list, flinging while it coasts. A nested
 * scrolling parent, like a collapsing toolbar, gets its share of every move before and after
 * the list, and what nobody takes pulls the edge effect.
 */
internal class SLKScrollGesture(private val list: SLKListView) {
  private val configuration = ViewConfiguration.get(list.context)
  private val touchSlop = configuration.scaledTouchSlop
  private val minFlingVelocity = configuration.scaledMinimumFlingVelocity
  private val maxFlingVelocity = configuration.scaledMaximumFlingVelocity
  private val scroller = OverScroller(list.context)
  private var velocityTracker: VelocityTracker? = null
  private val edges = SLKEdgeEffects(list)
  private val consumed = IntArray(2)
  private val offsetInWindow = IntArray(2)

  // A finger moves the list.
  var isTracking = false
    private set

  // The list coasts after a fling.
  var isFlinging = false
    private set

  // The scroller runs an animated scroll command.
  private var isAnimating = false

  /*
   * A free fling hands its moves to a nested parent too. The scroller then runs unbounded and
   * only its steps count. A snapping fling and animated commands go to fixed offsets instead.
   */
  private var flingSteps = false
  private var previousFlingPosition = 0

  private var activePointer = MotionEvent.INVALID_POINTER_ID
  private var downX = 0f
  private var downY = 0f
  private var previousAlong = 0f
  private var previousCross = 0f

  // A touch that stopped a fling never counts as a tap.
  private var caughtScroll = false
  private var moved = false

  private val axis: Int get() = if (list.horizontal) ViewCompat.SCROLL_AXIS_HORIZONTAL else ViewCompat.SCROLL_AXIS_VERTICAL

  /*
   * Every event passes here first, to tell taps from scrolls.
   */
  fun observe(event: MotionEvent) {
    trackVelocity(event)
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        downX = event.x
        downY = event.y
        moved = false
        caughtScroll = isFlinging || isAnimating
        if (!caughtScroll) list.highlightDown(event.x, event.y)
      }
      MotionEvent.ACTION_MOVE -> {
        if (!moved && (abs(event.x - downX) > touchSlop || abs(event.y - downY) > touchSlop)) {
          moved = true
          list.cancelHighlight()
        }
      }
      MotionEvent.ACTION_UP -> {
        if (!moved && !caughtScroll && !isTracking) list.handleTap(event.x, event.y)
        list.cancelHighlight()
      }
      MotionEvent.ACTION_CANCEL -> list.cancelHighlight()
    }
  }

  fun intercept(event: MotionEvent): Boolean {
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> beginTouch(event)
      MotionEvent.ACTION_MOVE -> if (!isTracking) startTrackingIfMoved(event)
      MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> endTouch(false)
    }
    return isTracking
  }

  fun touch(event: MotionEvent): Boolean {
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> beginTouch(event)
      MotionEvent.ACTION_MOVE -> moveTouch(event)
      MotionEvent.ACTION_UP -> endTouch(true)
      MotionEvent.ACTION_CANCEL -> endTouch(false)
    }
    return true
  }

  private fun beginTouch(event: MotionEvent) {
    activePointer = event.getPointerId(0)
    previousAlong = list.along(event.x, event.y)
    previousCross = list.cross(event.x, event.y)
    list.gestureBegan()
    list.startNestedScroll(axis, ViewCompat.TYPE_TOUCH)
    if (!scroller.isFinished) {
      // A touch on a moving list stops it and takes over right away. It holds nothing.
      list.scrollTrackingStarted()
      scroller.abortAnimation()
      endFlingSteps()
      isFlinging = false
      isAnimating = false
      isTracking = true
      list.parent?.requestDisallowInterceptTouchEvent(true)
    }
  }

  private fun startTrackingIfMoved(event: MotionEvent) {
    val pointer = event.findPointerIndex(activePointer)
    if (pointer < 0) return
    val along = list.along(event.getX(pointer), event.getY(pointer))
    val cross = list.cross(event.getX(pointer), event.getY(pointer))
    val alongDelta = abs(along - list.along(downX, downY))
    val crossDelta = abs(cross - list.cross(downX, downY))
    if (alongDelta > touchSlop && alongDelta > crossDelta) {
      isTracking = true
      previousAlong = along
      list.parent?.requestDisallowInterceptTouchEvent(true)
      list.scrollTrackingStarted()
    }
  }

  private fun moveTouch(event: MotionEvent) {
    if (!isTracking) {
      startTrackingIfMoved(event)
      return
    }
    val pointer = event.findPointerIndex(activePointer)
    if (pointer < 0) return
    val along = list.along(event.getX(pointer), event.getY(pointer))
    previousCross = list.cross(event.getX(pointer), event.getY(pointer))
    var delta = (previousAlong - along).roundToInt()
    if (delta == 0) return
    previousAlong -= delta
    // A pulled refresh spinner takes a move back toward the content first.
    if (delta > 0 && list.refresh.isPulling) {
      delta = list.refresh.pushBack(delta)
      if (delta == 0) return
    }
    val left = scrollWithParent(delta, ViewCompat.TYPE_TOUCH)
    if (left < 0 && list.canPullToRefresh) {
      list.refresh.pullBy(-left)
    } else if (left != 0) {
      edges.pull(left, previousCross)
    } else if (!edges.isFinished) {
      edges.release()
    }
  }

  /*
   * Move by delta along the axis. A nested parent takes its share first, the list takes what
   * fits before its ends, and the parent gets the rest. Returns what nobody took.
   */
  private fun scrollWithParent(delta: Int, type: Int): Int {
    var remaining = delta
    consumed[0] = 0
    consumed[1] = 0
    if (list.dispatchNestedPreScroll(alongX(remaining), alongY(remaining), consumed, offsetInWindow, type)) {
      remaining -= alongOf(consumed)
      followParent(type)
    }
    val before = list.offset
    val next = min(max(before + remaining, 0), list.maxOffset)
    if (next != before) {
      if (list.horizontal) list.scrollTo(next, 0) else list.scrollTo(0, next)
    }
    val taken = next - before
    remaining -= taken
    consumed[0] = 0
    consumed[1] = 0
    list.dispatchNestedScroll(alongX(taken), alongY(taken), alongX(remaining), alongY(remaining), offsetInWindow, type, consumed)
    remaining -= alongOf(consumed)
    followParent(type)
    return remaining
  }

  /*
   * The parent moved the list on screen. The finger stays put on screen and moved the other way
   * in the list's coordinates.
   */
  private fun followParent(type: Int) {
    if (type == ViewCompat.TYPE_TOUCH) previousAlong -= alongOf(offsetInWindow)
    offsetInWindow[0] = 0
    offsetInWindow[1] = 0
  }

  private fun alongX(value: Int): Int = if (list.horizontal) value else 0
  private fun alongY(value: Int): Int = if (list.horizontal) 0 else value
  private fun alongOf(pair: IntArray): Int = if (list.horizontal) pair[0] else pair[1]

  private fun endTouch(allowFling: Boolean) {
    val wasTracking = isTracking
    isTracking = false
    activePointer = MotionEvent.INVALID_POINTER_ID
    edges.release()
    list.stopNestedScroll(ViewCompat.TYPE_TOUCH)
    if (list.refresh.isPulling) list.refreshReleased()
    if (wasTracking) {
      val velocity = currentVelocity()
      if (!(allowFling && flingWithParent(velocity))) settleWithoutFling()
    }
  }

  /*
   * A down starts a fresh velocity, the rest of the touch adds to it.
   */
  private fun trackVelocity(event: MotionEvent) {
    if (event.actionMasked == MotionEvent.ACTION_DOWN) velocityTracker?.clear()
    val tracker = velocityTracker ?: VelocityTracker.obtain().also { velocityTracker = it }
    tracker.addMovement(event)
  }

  private fun currentVelocity(): Float {
    val tracker = velocityTracker ?: return 0f
    tracker.computeCurrentVelocity(1000, maxFlingVelocity.toFloat())
    return -(if (list.horizontal) tracker.xVelocity else tracker.yVelocity)
  }

  /*
   * A nested parent may take the whole fling. Otherwise it hears about it and the list coasts.
   */
  private fun flingWithParent(velocity: Float): Boolean {
    if (abs(velocity) < minFlingVelocity) return false
    val vx = if (list.horizontal) velocity else 0f
    val vy = if (list.horizontal) 0f else velocity
    if (list.dispatchNestedPreFling(vx, vy)) return false
    list.dispatchNestedFling(vx, vy, true)
    return fling(velocity)
  }

  /*
   * Coast with the finger's velocity. A snapping list ends the fling on a row edge.
   */
  private fun fling(velocity: Float): Boolean {
    val start = list.offset
    if (list.horizontal) {
      scroller.fling(start, 0, velocity.roundToInt(), 0, 0, list.maxOffset, 0, 0)
    } else {
      scroller.fling(0, start, 0, velocity.roundToInt(), 0, 0, 0, list.maxOffset)
    }
    val end = if (list.horizontal) scroller.finalX else scroller.finalY
    val snapped = list.snapTarget(end)
    scroller.abortAnimation()
    if (snapped != end) {
      startScroll(start, snapped, snapDuration(abs(snapped - start), abs(velocity)))
    } else {
      startFlingSteps(velocity)
    }
    isFlinging = true
    list.postInvalidateOnAnimation()
    return true
  }

  /*
   * A free fling runs unbounded. Each frame's step goes through scrollWithParent, which lets a
   * parent take the part past the list's ends.
   */
  private fun startFlingSteps(velocity: Float) {
    flingSteps = true
    previousFlingPosition = 0
    list.startNestedScroll(axis, ViewCompat.TYPE_NON_TOUCH)
    val bound = Int.MAX_VALUE / 2
    if (list.horizontal) {
      scroller.fling(0, 0, velocity.roundToInt(), 0, -bound, bound, 0, 0)
    } else {
      scroller.fling(0, 0, 0, velocity.roundToInt(), 0, 0, -bound, bound)
    }
  }

  private fun endFlingSteps() {
    if (!flingSteps) return
    flingSteps = false
    list.stopNestedScroll(ViewCompat.TYPE_NON_TOUCH)
  }

  /*
   * A decelerating move covers its distance in about twice the time the start velocity would.
   */
  private fun snapDuration(distance: Int, velocity: Float): Int =
    (2000f * distance / velocity).roundToInt().coerceIn(200, 800)

  private fun settleWithoutFling() {
    val snapped = list.snapTarget(list.offset)
    if (snapped != list.offset) {
      isFlinging = true
      startScroll(list.offset, snapped, 250)
      list.postInvalidateOnAnimation()
      return
    }
    list.scrollingEnded()
  }

  private fun startScroll(from: Int, to: Int, duration: Int) {
    if (list.horizontal) scroller.startScroll(from, 0, to - from, 0, duration)
    else scroller.startScroll(0, from, 0, to - from, duration)
  }

  /*
   * Animate to an offset for a scroll command. It ends in list.scrollingEnded.
   */
  fun animateTo(target: Int) {
    scroller.abortAnimation()
    endFlingSteps()
    isFlinging = false
    isAnimating = true
    startScroll(list.offset, target, 300)
    list.postInvalidateOnAnimation()
  }

  fun computeScroll() {
    if (!isFlinging && !isAnimating) return
    if (scroller.computeScrollOffset()) {
      val position = if (list.horizontal) scroller.currX else scroller.currY
      if (flingSteps) {
        stepFling(position)
      } else if (position != list.offset) {
        if (list.horizontal) list.scrollTo(position, 0) else list.scrollTo(0, position)
      }
      if (!scroller.isFinished) {
        list.postInvalidateOnAnimation()
        return
      }
    }
    endFlingSteps()
    isFlinging = false
    isAnimating = false
    list.scrollingEnded()
  }

  /*
   * One frame of a free fling. A step nobody takes means it hit an end: the edge absorbs the
   * velocity and the fling stops.
   */
  private fun stepFling(position: Int) {
    val delta = position - previousFlingPosition
    previousFlingPosition = position
    if (delta == 0) return
    val left = scrollWithParent(delta, ViewCompat.TYPE_NON_TOUCH)
    if (left != 0) {
      edges.absorb(if (left < 0) -abs(scroller.currVelocity) else abs(scroller.currVelocity))
      scroller.abortAnimation()
    }
  }

  /*
   * Draw the edge effects over the content. Returns whether they need another frame.
   */
  fun drawEdges(canvas: android.graphics.Canvas): Boolean = edges.draw(canvas)

  /*
   * A swipe or a menu took the touch. Let go of it without a fling or a tap.
   */
  fun abandon() {
    moved = true
    isTracking = false
    activePointer = MotionEvent.INVALID_POINTER_ID
    edges.release()
    list.stopNestedScroll(ViewCompat.TYPE_TOUCH)
  }

  fun stop() {
    if (scroller.isFinished) return
    scroller.abortAnimation()
    endFlingSteps()
    isFlinging = false
    isAnimating = false
  }
}
