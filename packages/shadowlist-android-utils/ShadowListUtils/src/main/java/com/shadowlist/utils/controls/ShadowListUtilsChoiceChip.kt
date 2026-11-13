package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.widget.FrameLayout
import androidx.annotation.ColorInt

class ShadowListUtilsChoiceChip(context: Context, label: String, @ColorInt tint: Int, selected: Boolean, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  private lateinit var content: ShadowListUtilsChipContent
  private val check = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.CHECK).apply { color = theme.colors.label; importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO }
  var label = label
    set(value) { field = value; refresh() }
  @get:ColorInt
  @setparam:ColorInt
  var tint = tint
    set(value) { field = value; refresh() }
  var onPress: (() -> Unit)? = null
    set(value) { field = value; refresh() }

  init {
    content = ShadowListUtilsChipContent(context, theme); addView(content)
    isSelected = selected; refresh()
  }
  override fun setSelected(selected: Boolean) { super.setSelected(selected); if (::content.isInitialized) refresh() }
  override fun onConfigurationChanged(newConfig: Configuration) { super.onConfigurationChanged(newConfig); refresh() }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    content.measure(widthMeasureSpec, heightMeasureSpec); setMeasuredDimension(content.measuredWidth, content.measuredHeight)
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) { content.layout(0, 0, width, height) }
  private fun refresh() {
    content.label = label; content.onPress = onPress; content.isSelected = isSelected
    content.fillOverride = tint; content.textColorOverride = theme.colors.label
    content.borderWidth = 2f; content.borderColor = if (isSelected) theme.colors.label else android.graphics.Color.TRANSPARENT
    content.leadingView = if (isSelected) check else null
    content.refresh(); requestLayout()
  }
}
