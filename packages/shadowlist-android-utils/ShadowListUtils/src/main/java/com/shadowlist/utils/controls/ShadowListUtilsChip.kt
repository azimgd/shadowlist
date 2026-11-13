package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.widget.FrameLayout

/* Optional press and clear are independent. Inherited isSelected remains application-owned. */
class ShadowListUtilsChip(context: Context, label: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  private lateinit var content: ShadowListUtilsChipContent
  var label = label
    set(value) { field = value; refresh() }
  var onPress: (() -> Unit)? = null
    set(value) { field = value; refresh() }
  var onClear: (() -> Unit)? = null
    set(value) { field = value; refresh() }
  var variant = ShadowListUtilsChipVariant.TINTED
    set(value) { field = value; refresh() }
  var size = ShadowListUtilsChipSize.REGULAR
    set(value) { field = value; refresh() }
  var mono = false
    set(value) { field = value; refresh() }
  var maxFontSizeMultiplier: Float? = null
    set(value) { field = value; refresh() }
  var accessibilityLabel: String? = null
    set(value) { field = value; refresh() }
  var clearAccessibilityLabel: String? = null
    set(value) { field = value; refresh() }

  init { content = ShadowListUtilsChipContent(context, theme); addView(content); refresh() }
  override fun setSelected(selected: Boolean) { super.setSelected(selected); if (::content.isInitialized) refresh() }
  override fun onConfigurationChanged(newConfig: Configuration) { super.onConfigurationChanged(newConfig); refresh() }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    content.measure(widthMeasureSpec, heightMeasureSpec); setMeasuredDimension(content.measuredWidth, content.measuredHeight)
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) { content.layout(0, 0, width, height) }
  private fun refresh() {
    content.label = label; content.onPress = onPress; content.onClear = onClear
    content.isSelected = isSelected; content.variant = variant; content.size = size; content.mono = mono
    content.maxFontSizeMultiplier = maxFontSizeMultiplier
    content.labelOverride = accessibilityLabel; content.clearLabelOverride = clearAccessibilityLabel
    content.refresh(); requestLayout()
  }
}
