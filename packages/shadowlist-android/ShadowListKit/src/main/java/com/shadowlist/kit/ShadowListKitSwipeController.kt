package com.shadowlist.kit

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Color
import android.graphics.Rect
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View
import android.view.ViewConfiguration
import android.view.ViewGroup
import android.view.animation.DecelerateInterpolator
import android.widget.TextView
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.roundToInt

/*
 * Swipe actions. A move across the scroll axis on a row with actions moves the row and shows
 * its buttons behind it. The core's SwipeReveal in the host layer decides how far the row
 * follows and where it rests. A touch anywhere else closes an open row, and a drag from there
 * still scrolls.
 */
internal class ShadowListKitSwipeController(private val list: ShadowListKitListView) {
  private val touchSlop = ViewConfiguration.get(list.context).scaledTouchSlop
  private val spec = DoubleArray(ShadowListKitCore.SWIPE_SLOTS)
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
    candidate === cell && ShadowListKitCore.swipeIsOut(spec, offset.toDouble())

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
    if (actionsView?.hasButtonAt(event.x + list.scrollX, event.y + list.scrollY) == true) {
      buttonTouch = true
      return false
    }
    if (list.itemCellAt(event.x, event.y) !== open) {
      close(true)
      closingTouch = true
      list.selection.cancelHighlight()
    }
    return false
  }

  private fun move(event: MotionEvent): Boolean {
    if (closingTouch) return false
    if (buttonTouch) return false
    track(event)
    if (!tracking) {
      val crossDelta = list.cross(event.x, event.y) - list.cross(downX, downY)
      val alongDelta = list.along(event.x, event.y) - list.along(downX, downY)
      if (abs(crossDelta) <= touchSlop || abs(crossDelta) <= abs(alongDelta) || !begin(crossDelta)) return false
      tracking = true
      list.abandonScrollGesture()
      list.cancelChildTouches()
      startOffset = offset
    }
    val translation = list.cross(event.x, event.y) - list.cross(downX, downY)
    val wasPast = pastFull(offset)
    apply(ShadowListKitCore.swipeDrag(spec, startOffset.toDouble(), translation.toDouble()).toFloat())
    if (pastFull(offset) != wasPast) cell?.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
    return true
  }

  private fun up(event: MotionEvent): Boolean {
    // The flag lasts until the next touch. The tap it would make is dropped on the way.
    if (closingTouch) return false
    if (tracking) {
      tracking = false
      track(event)
      val tracker = velocityTracker
      tracker?.computeCurrentVelocity(1000)
      val velocity = if (tracker == null) 0f else if (list.horizontal) tracker.yVelocity else tracker.xVelocity
      settle(velocity)
      return true
    }
    // A tap on the open row closes it.
    val open = cell
    if (open != null && !buttonTouch && event.actionMasked == MotionEvent.ACTION_UP &&
      list.itemCellAt(event.x, event.y) === open) {
      close(true)
      return true
    }
    return false
  }

  private fun track(event: MotionEvent) {
    val tracker = velocityTracker ?: VelocityTracker.obtain().also { velocityTracker = it }
    tracker.addMovement(event)
  }

  private fun pastFull(value: Float): Boolean = value != 0f && ShadowListKitCore.swipePastFull(spec, value.toDouble())

  /*
   * Start swiping the row under the touch's start toward a side that has actions.
   */
  private fun begin(crossDelta: Float): Boolean {
    if (list.editing || list.hasHeldRow) return false
    val target = list.itemCellAt(downX, downY) ?: return false
    if (target === cell) {
      animator?.cancel()
      return true
    }
    val leading = actions(target, leadingSide = true)
    val trailing = actions(target, leadingSide = false)
    if ((if (crossDelta > 0) leading else trailing) == null) return false
    close(false)
    open(target, leading, trailing)
    return true
  }

  private fun actions(target: ShadowListKitListCell, leadingSide: Boolean): ShadowListKitSwipeActionsConfiguration? {
    val delegate = list.delegate ?: return null
    val configuration = if (leadingSide) delegate.leadingSwipeActionsForItem(list, target.index)
      else delegate.trailingSwipeActionsForItem(list, target.index)
    return configuration?.takeIf { it.actions.isNotEmpty() }
  }

  private fun open(target: ShadowListKitListCell, leading: ShadowListKitSwipeActionsConfiguration?, trailing: ShadowListKitSwipeActionsConfiguration?) {
    val view = ShadowListKitSwipeActionsView(list.context, leading, trailing, list.horizontal, list.density) { perform(it) }
    list.addSwipeActionsView(view)
    view.measure(
      View.MeasureSpec.makeMeasureSpec(target.width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(target.height, View.MeasureSpec.EXACTLY))
    view.layout(target.left, target.top, target.right, target.bottom)
    spec[ShadowListKitCore.SWIPE_LEADING_WIDTH] = view.leadingWidth
    spec[ShadowListKitCore.SWIPE_TRAILING_WIDTH] = view.trailingWidth
    spec[ShadowListKitCore.SWIPE_LEADING_FULL] = if (leading?.performsFirstActionWithFullSwipe == true) 1.0 else 0.0
    spec[ShadowListKitCore.SWIPE_TRAILING_FULL] = if (trailing?.performsFirstActionWithFullSwipe == true) 1.0 else 0.0
    spec[ShadowListKitCore.SWIPE_ROW_SIZE] = (if (list.horizontal) target.height else target.width).toDouble()
    cell = target
    actionsView = view
    offset = 0f
    if (target.highlighted) target.setHighlighted(false, false)
  }

  private fun apply(value: Float) {
    offset = value
    val target = cell ?: return
    if (list.horizontal) target.translationY = value else target.translationX = value
    actionsView?.layoutFor(value, pastFull(value))
  }

  private fun settle(velocity: Float) {
    ShadowListKitCore.swipeSettle(spec, offset.toDouble(), velocity.toDouble(), ShadowListKitCore.SWIPE_FLING_VELOCITY_DP * list.density)
    val side = spec[ShadowListKitCore.SWIPE_OUT_SIDE].toInt()
    val full = spec[ShadowListKitCore.SWIPE_OUT_FULL] != 0.0
    animateTo(spec[ShadowListKitCore.SWIPE_OUT_OFFSET].toFloat())
    if (full) {
      val view = actionsView ?: return
      val configuration = if (side == ShadowListKitCore.SWIPE_SIDE_LEADING) view.leading else view.trailing
      configuration?.actions?.firstOrNull()?.let(::perform)
    }
  }

  private fun animateTo(target: Float) {
    animator?.cancel()
    val swiped = cell ?: return
    animator = ValueAnimator.ofFloat(offset, target).apply {
      duration = ShadowListKitCore.SWIPE_DURATION_MS
      interpolator = DecelerateInterpolator()
      addUpdateListener { if (cell === swiped) apply(it.animatedValue as Float) }
      addListener(object : AnimatorListenerAdapter() {
        override fun onAnimationEnd(animation: Animator) {
          if (cell === swiped && target == 0f) tearDown()
        }
      })
      start()
    }
  }

  /*
   * Run an action. Its completion closes the row, unless the action deleted it.
   */
  private fun perform(action: ShadowListKitSwipeAction) {
    val swiped = cell ?: return
    action.handler(action) { _ ->
      list.post { if (cell === swiped) close(true) }
    }
  }

  fun close(animated: Boolean) {
    if (cell == null) return
    if (animated) {
      animateTo(0f)
      return
    }
    animator?.cancel()
    apply(0f)
    tearDown()
  }

  private fun tearDown() {
    animator?.cancel()
    animator = null
    actionsView?.let { list.removeSwipeActionsView(it) }
    actionsView = null
    cell?.let {
      if (list.horizontal) it.translationY = 0f else it.translationX = 0f
    }
    cell = null
    offset = 0f
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
    if (swiped.visibility != View.VISIBLE || swiped.row < 0) {
      tearDown()
      return
    }
    if (view.left != swiped.left || view.top != swiped.top || view.width != swiped.width || view.height != swiped.height) {
      view.measure(
        View.MeasureSpec.makeMeasureSpec(swiped.width, View.MeasureSpec.EXACTLY),
        View.MeasureSpec.makeMeasureSpec(swiped.height, View.MeasureSpec.EXACTLY))
      view.layout(swiped.left, swiped.top, swiped.right, swiped.bottom)
      view.layoutFor(offset, pastFull(offset))
    }
  }
}

/*
 * The buttons behind a swiped row, for both sides. It sits under the row's cell with the
 * cell's frame. The side being revealed shows its buttons stretched over the gap the row
 * leaves. Past the full swipe point the first button fills all of it.
 */
internal class ShadowListKitSwipeActionsView(
  context: Context,
  val leading: ShadowListKitSwipeActionsConfiguration?,
  val trailing: ShadowListKitSwipeActionsConfiguration?,
  private val horizontal: Boolean,
  density: Float,
  private val onAction: (ShadowListKitSwipeAction) -> Unit,
) : ViewGroup(context) {
  private val leadingButtons = buttons(leading)
  private val trailingButtons = buttons(trailing)
  private val leadingSizes = sizes(leadingButtons, density)
  private val trailingSizes = sizes(trailingButtons, density)
  private val clip = Rect()

  /*
   * The revealed span, then each button's start and size, from the core.
   */
  private val spans = DoubleArray(2 + 2 * max(leadingButtons.size, trailingButtons.size))

  val leadingWidth: Double get() = leadingSizes.sum()
  val trailingWidth: Double get() = trailingSizes.sum()

  private fun buttons(configuration: ShadowListKitSwipeActionsConfiguration?): List<TextView> =
    configuration?.actions?.map { action ->
      TextView(context).apply {
        text = action.title
        setTextColor(Color.WHITE)
        textSize = 15f
        gravity = Gravity.CENTER
        maxLines = 1
        setBackgroundColor(action.shownColor)
        action.image?.let { setCompoundDrawablesWithIntrinsicBounds(null, it, null, null) }
        contentDescription = action.title
        setOnClickListener { onAction(action) }
        addView(this)
      }
    } ?: emptyList()

  private fun sizes(buttons: List<TextView>, density: Float): DoubleArray = DoubleArray(buttons.size) {
    val button = buttons[it]
    button.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED)
    val fitted = if (horizontal) button.measuredHeight else button.measuredWidth
    ShadowListKitCore.swipeButtonSize(fitted.toDouble(), density.toDouble())
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {}

  /*
   * Place the buttons for a row moved offset across the axis. full lets the first button fill
   * the gap.
   */
  fun layoutFor(offset: Float, full: Boolean) {
    val leadingSide = offset > 0
    val shown = if (leadingSide) leadingButtons else trailingButtons
    val other = if (leadingSide) trailingButtons else leadingButtons
    val sizes = if (leadingSide) leadingSizes else trailingSizes
    for (button in other) button.visibility = INVISIBLE
    val crossSize = if (horizontal) height else width
    val alongSize = if (horizontal) width else height
    ShadowListKitCore.swipeButtonSpans(sizes, sizes.size, offset.toDouble(), full, crossSize.toDouble(), spans)
    for ((at, button) in shown.withIndex()) {
      // Each edge is rounded on its own, which keeps the buttons touching.
      val start = spans[2 + at * 2].roundToInt()
      val end = (spans[2 + at * 2] + spans[3 + at * 2]).roundToInt()
      val size = end - start
      button.visibility = if (size > 0) VISIBLE else INVISIBLE
      if (size > 0) {
        val w = if (horizontal) alongSize else size
        val h = if (horizontal) size else alongSize
        button.measure(MeasureSpec.makeMeasureSpec(w, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(h, MeasureSpec.EXACTLY))
        if (horizontal) button.layout(0, start, alongSize, end) else button.layout(start, 0, end, alongSize)
      }
    }
    shown.firstOrNull()?.let { setBackgroundColor((it.background as? ColorDrawable)?.color ?: 0) }
    // Only the gap shows the buttons' color.
    val gapStart = spans[0].roundToInt()
    val gapEnd = (spans[0] + spans[1]).roundToInt()
    if (horizontal) clip.set(0, gapStart, alongSize, gapEnd) else clip.set(gapStart, 0, gapEnd, alongSize)
    clipBounds = clip
  }

  /*
   * Whether a point in the list's scrolled coordinates hits a shown button.
   */
  fun hasButtonAt(x: Float, y: Float): Boolean {
    val localX = (x - left).toInt()
    val localY = (y - top).toInt()
    for (button in leadingButtons + trailingButtons) {
      if (button.visibility == VISIBLE && localX >= button.left && localX < button.right &&
        localY >= button.top && localY < button.bottom) return true
    }
    return false
  }
}
