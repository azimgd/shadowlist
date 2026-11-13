package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import android.widget.LinearLayout

class ShadowListUtilsSegmented(context: Context, options: List<ShadowListUtilsSegmentedOption>, value: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  var options = options.toList()
    set(value) { field = value.toList(); rebuild() }
  var value = value
    set(value) { field = value; refresh() }
  var onChange: ((String) -> Unit)? = null
  var size = ShadowListUtilsSegmentedSize.REGULAR
    set(value) { field = value; refresh() }
  var accessibilityLabel: String? = null
    set(value) { field = value; contentDescription = value }
  private val track = LinearLayout(context).apply {
    orientation = LinearLayout.HORIZONTAL; setPadding(2f.px, 2f.px, 2f.px, 2f.px)
    clipChildren = false; clipToPadding = false
  }
  private var buttons = emptyList<ShadowListUtilsSegmentedButton>()

  init {
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
    clipChildren = false; clipToPadding = false
    addView(track, LayoutParams(LayoutParams.MATCH_PARENT, 36f.px, Gravity.START or Gravity.TOP))
    rebuild()
  }
  override fun onConfigurationChanged(newConfig: Configuration) { super.onConfigurationChanged(newConfig); refresh() }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec((if (size == ShadowListUtilsSegmentedSize.COMPACT) 34f else 36f).px, MeasureSpec.EXACTLY))
  }
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    info.className = android.widget.TabWidget::class.java.name
    info.collectionInfo = AccessibilityNodeInfo.CollectionInfo.obtain(1, options.size, false, AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_SINGLE)
  }
  private fun rebuild() {
    val available = buttons.toMutableList()
    buttons = options.map { option ->
      val previous = available.indexOfFirst { it.option.id == option.id }
      val button = if (previous >= 0) available.removeAt(previous) else ShadowListUtilsSegmentedButton(context, option, theme)
      button.option = option
      button.onPress = { onChange?.invoke(button.option.id) }
      button
    }
    track.replaceChildren(buttons)
    refresh()
  }
  private fun refresh() {
    val compact = size == ShadowListUtilsSegmentedSize.COMPACT
    track.background = GradientDrawable().apply {
      setColor(if (compact) theme.colors.fill else theme.colors.elevated2)
      cornerRadius = (if (compact) 8f else 9f).pxf
    }
    track.layoutParams = (track.layoutParams as LayoutParams).apply { width = if (compact) LayoutParams.WRAP_CONTENT else LayoutParams.MATCH_PARENT; height = (if (compact) 34f else 36f).px }
    val selected = options.indexOfFirst { it.id == value }
    buttons.forEachIndexed { index, button ->
      button.compact = compact; button.index = index; button.isSelected = selected == index
      button.divider.visibility = if (index == 0 || selected == index || selected == index - 1) View.GONE else View.VISIBLE
      button.layoutParams = LinearLayout.LayoutParams(if (compact) LayoutParams.WRAP_CONTENT else 0, LayoutParams.MATCH_PARENT, if (compact) 0f else 1f)
      button.refresh()
    }
    requestLayout()
  }
}
