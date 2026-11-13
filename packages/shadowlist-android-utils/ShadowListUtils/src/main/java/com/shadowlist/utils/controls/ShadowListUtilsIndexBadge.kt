package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.widget.TextView

class ShadowListUtilsIndexBadge(context: Context, index: Int, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : TextView(context) {
  var index = index
    set(value) { field = value; text = value.toString(); contentDescription = value.toString() }
  init {
    text = index.toString(); contentDescription = index.toString(); gravity = Gravity.CENTER
    textSize = 12f; typeface = ShadowListUtilsTextStyle.typeface(ShadowListUtilsTextStyle.WEIGHT_SEMIBOLD)
    fontFeatureSettings = "tnum"; setTextColor(theme.colors.accent)
    background = GradientDrawable().apply { setColor(theme.colors.accentSoft); cornerRadius = 6f.pxf }
  }
  override fun onConfigurationChanged(newConfig: android.content.res.Configuration) { super.onConfigurationChanged(newConfig); textSize = 12f }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) { setMeasuredDimension(22f.px, 22f.px) }
}
