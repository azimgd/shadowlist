package com.shadowlist.kit

import android.graphics.Canvas
import android.view.View
import android.widget.EdgeEffect
import kotlin.math.abs

/*
 * The platform's edge effect at both ends of the list: a glow, or a stretch on Android 12 and
 * later. A drag past an end pulls it, a fling that hits an end absorbs its velocity. It follows
 * the scroll axis and the view's overScrollMode.
 */
internal class SLKEdgeEffects(private val list: SLKListView) {
  private val start = EdgeEffect(list.context)
  private val end = EdgeEffect(list.context)

  private val enabled: Boolean
    get() = when (list.overScrollMode) {
      View.OVER_SCROLL_NEVER -> false
      View.OVER_SCROLL_IF_CONTENT_SCROLLS -> list.maxOffset > 0
      else -> true
    }

  /*
   * A drag went past an end by distance pixels, negative past the start. cross is where the
   * finger is across the axis, in pixels.
   */
  fun pull(distance: Int, cross: Float) {
    if (distance == 0 || !enabled) return
    val window = list.windowAlong.coerceAtLeast(1).toFloat()
    val crossSize = (if (list.horizontal) list.height else list.width).coerceAtLeast(1).toFloat()
    val fraction = (cross / crossSize).coerceIn(0f, 1f)
    if (distance < 0) {
      // The start edge draws rotated for a horizontal list. Its cross position flips there.
      start.onPull(abs(distance) / window, if (list.horizontal) 1f - fraction else fraction)
      if (!end.isFinished) end.onRelease()
    } else {
      end.onPull(distance / window, if (list.horizontal) fraction else 1f - fraction)
      if (!start.isFinished) start.onRelease()
    }
    list.postInvalidateOnAnimation()
  }

  fun release() {
    start.onRelease()
    end.onRelease()
    list.postInvalidateOnAnimation()
  }

  /*
   * A fling hit an end with velocity in pixels per second, negative toward the start.
   */
  fun absorb(velocity: Float) {
    if (!enabled || velocity == 0f) return
    val effect = if (velocity < 0) start else end
    effect.onAbsorb(abs(velocity).toInt().coerceAtLeast(1))
    list.postInvalidateOnAnimation()
  }

  val isFinished: Boolean get() = start.isFinished && end.isFinished

  /*
   * Draw both edges over the content at the viewport's ends. Returns whether they still run.
   */
  fun draw(canvas: Canvas): Boolean {
    if (isFinished) return false
    val width = list.width
    val height = list.height
    var running = false
    if (!start.isFinished) {
      val save = canvas.save()
      canvas.translate(list.scrollX.toFloat(), list.scrollY.toFloat())
      if (list.horizontal) {
        canvas.rotate(270f)
        canvas.translate(-height.toFloat(), 0f)
        start.setSize(height, width)
      } else {
        start.setSize(width, height)
      }
      running = start.draw(canvas) || running
      canvas.restoreToCount(save)
    }
    if (!end.isFinished) {
      val save = canvas.save()
      canvas.translate(list.scrollX.toFloat(), list.scrollY.toFloat())
      if (list.horizontal) {
        canvas.rotate(90f)
        canvas.translate(0f, -width.toFloat())
        end.setSize(height, width)
      } else {
        canvas.rotate(180f)
        canvas.translate(-width.toFloat(), -height.toFloat())
        end.setSize(width, height)
      }
      running = end.draw(canvas) || running
      canvas.restoreToCount(save)
    }
    return running || !isFinished
  }
}
