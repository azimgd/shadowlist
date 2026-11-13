package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.View

/*
 * A hairline between two rows that starts after the leading inset, by default where a row's
 * text starts. The inset shows the theme's background. inset is in dp.
 */
class ShadowListUtilsItemSeparator(context: Context, theme: ShadowListUtilsTheme) : View(context) {
  var theme: ShadowListUtilsTheme = theme
    set(value) {
      field = value
      applyTheme()
    }

  var inset: Float = theme.rowInset
    set(value) {
      field = value
      invalidate()
    }

  private val paint = Paint()

  init {
    applyTheme()
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(
      getDefaultSize(suggestedMinimumWidth, widthMeasureSpec),
      resolveSize(ShadowListUtilsTheme.HAIRLINE_PX, heightMeasureSpec))
  }

  override fun onDraw(canvas: Canvas) {
    if (ShadowListUtilsPlatform.isRightToLeft) {
      canvas.drawRect(0f, 0f, width - inset.pxf, height.toFloat(), paint)
    } else {
      canvas.drawRect(inset.pxf, 0f, width.toFloat(), height.toFloat(), paint)
    }
  }

  private fun applyTheme() {
    setBackgroundColor(theme.colors.background)
    paint.color = theme.colors.separator
    invalidate()
  }
}
