package com.shadowlist.kit

import android.animation.ValueAnimator
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.SystemClock
import kotlin.math.max
import kotlin.math.min

/*
 * Pull to refresh without SwipeRefreshLayout. A drag past the start pulls a spinner in over the
 * rows. Let go past the trigger distance, it stays and spins until refreshing ends. Shorter, it
 * slides back. The rows do not move, like SwipeRefreshLayout.
 */
internal class SLKRefreshIndicator(private val list: SLKListView) {
  companion object {
    private const val TRIGGER_DP = 72f
    private const val MAX_PULL_DP = 120f
    private const val SIZE_DP = 36f
    private const val STROKE_DP = 3f
    private const val PULL_RESISTANCE = 0.5f
    private const val RETURN_DURATION_MS = 200L
    private const val SPIN_PERIOD_MS = 1000L
  }

  /*
   * Read from the resources. The list's own density field is not set yet while it builds this.
   */
  private val density = list.resources.displayMetrics.density
  private val circle = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE; setShadowLayer(4 * density, 0f, density, 0x40000000) }
  private val arc = Paint(Paint.ANTI_ALIAS_FLAG).apply {
    style = Paint.Style.STROKE
    strokeWidth = STROKE_DP * density
    strokeCap = Paint.Cap.ROUND
    color = Color.rgb(0, 122, 255)
  }
  private val bounds = RectF()
  private var animator: ValueAnimator? = null

  // How far the spinner is pulled in, in pixels.
  var pull = 0f
    private set

  var refreshing = false
    private set

  val isPulling: Boolean get() = pull > 0f && !refreshing

  var color: Int
    get() = arc.color
    set(value) { arc.color = value }

  /*
   * The finger moved past the start by distance pixels. Returns the part it took.
   */
  fun pullBy(distance: Int): Int {
    if (refreshing) return 0
    animator?.cancel()
    pull = min(MAX_PULL_DP * density, max(0f, pull + distance * PULL_RESISTANCE))
    list.postInvalidateOnAnimation()
    return distance
  }

  /*
   * The finger moved back toward the start while pulled. Returns the part of distance the pull
   * did not take.
   */
  fun pushBack(distance: Int): Int {
    if (pull <= 0f || refreshing) return distance
    val taken = min(pull, distance * PULL_RESISTANCE)
    pull -= taken
    list.postInvalidateOnAnimation()
    return distance - (taken / PULL_RESISTANCE).toInt()
  }

  /*
   * The finger let go. Returns whether that started refreshing.
   */
  fun release(): Boolean {
    if (refreshing || pull <= 0f) return false
    if (pull >= TRIGGER_DP * density) {
      refreshing = true
      animateTo(TRIGGER_DP * density)
      return true
    }
    animateTo(0f)
    return false
  }

  fun setRefreshing(value: Boolean) {
    if (value == refreshing) return
    refreshing = value
    animateTo(if (value) TRIGGER_DP * density else 0f)
  }

  private fun animateTo(target: Float) {
    animator?.cancel()
    animator = ValueAnimator.ofFloat(pull, target).apply {
      duration = RETURN_DURATION_MS
      addUpdateListener {
        pull = it.animatedValue as Float
        list.postInvalidateOnAnimation()
      }
      start()
    }
  }

  /*
   * Draw over the rows at the start of the viewport. Returns whether it needs another frame.
   */
  fun draw(canvas: Canvas): Boolean {
    if (pull <= 0f) return false
    val size = SIZE_DP * density
    val leading = pull - size
    val crossCenter = (if (list.horizontal) list.height else list.width) / 2f
    val cx = list.scrollX + if (list.horizontal) list.paddingLeft + leading + size / 2 else crossCenter
    val cy = list.scrollY + if (list.horizontal) crossCenter else list.paddingTop + leading + size / 2
    canvas.drawCircle(cx, cy, size / 2, circle)
    val radius = size / 2 - STROKE_DP * density * 2
    bounds.set(cx - radius, cy - radius, cx + radius, cy + radius)
    if (refreshing) {
      val turn = (SystemClock.uptimeMillis() % SPIN_PERIOD_MS) * 360f / SPIN_PERIOD_MS
      canvas.drawArc(bounds, turn, 270f, false, arc)
      return true
    }
    val progress = min(1f, pull / (TRIGGER_DP * density))
    arc.alpha = (80 + 175 * progress).toInt()
    canvas.drawArc(bounds, -90f, 300f * progress, false, arc)
    arc.alpha = 255
    return false
  }
}
