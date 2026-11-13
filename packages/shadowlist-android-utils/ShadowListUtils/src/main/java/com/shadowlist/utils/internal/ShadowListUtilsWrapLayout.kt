package com.shadowlist.utils

import android.graphics.RectF
import android.util.SizeF

/* Typed copy of shared leading-to-trailing wrap frames, in the caller's measured units. */
internal class ShadowListUtilsWrapLayout(width: Float, sizes: List<SizeF>, gap: Float) {
  val frames: List<RectF>
  val height: Float

  init {
    val measured = DoubleArray(sizes.size * ShadowListUtilsCore.WRAP_SIZE_SLOTS)
    for ((index, size) in sizes.withIndex()) {
      val offset = index * ShadowListUtilsCore.WRAP_SIZE_SLOTS
      measured[offset + ShadowListUtilsCore.WRAP_SIZE_WIDTH] = size.width.toDouble()
      measured[offset + ShadowListUtilsCore.WRAP_SIZE_HEIGHT] = size.height.toDouble()
    }
    val layout = ShadowListUtilsCore.wrapLayout(width.toDouble(), measured, gap.toDouble())
    height = layout[ShadowListUtilsCore.WRAP_OUT_HEIGHT].toFloat()
    frames = (ShadowListUtilsCore.WRAP_OUT_FRAMES until layout.size step ShadowListUtilsCore.WRAP_FRAME_SLOTS).map { offset ->
      val x = layout[offset + ShadowListUtilsCore.WRAP_FRAME_X].toFloat()
      val y = layout[offset + ShadowListUtilsCore.WRAP_FRAME_Y].toFloat()
      RectF(x, y, x + layout[offset + ShadowListUtilsCore.WRAP_FRAME_WIDTH].toFloat(), y + layout[offset + ShadowListUtilsCore.WRAP_FRAME_HEIGHT].toFloat())
    }
  }
}
