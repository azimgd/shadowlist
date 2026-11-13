package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import android.widget.TextView

internal class ShadowListUtilsSegmentedButton(context: Context, var option: ShadowListUtilsSegmentedOption, val theme: ShadowListUtilsTheme) : FrameLayout(context) {
  var onPress: (() -> Unit)? = null
  var compact = false
  var index = 0
  val label = TextView(context)
  val divider = View(context)

  init {
    isClickable = true; isFocusable = true; importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
    clipChildren = false; clipToPadding = false
    label.gravity = Gravity.CENTER; label.setSingleLine(true); label.ellipsize = android.text.TextUtils.TruncateAt.END
    label.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    divider.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO; divider.setBackgroundColor(theme.colors.separator)
    addView(label, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER))
    addView(divider, LayoutParams(1, LayoutParams.MATCH_PARENT, Gravity.START).apply { topMargin = 7f.px; bottomMargin = 7f.px })
    setOnClickListener { onPress?.invoke() }
  }
  fun refresh() {
    contentDescription = option.label
    label.text = option.label; label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
    label.typeface = ShadowListUtilsTextStyle.typeface(if (isSelected) ShadowListUtilsTextStyle.WEIGHT_SEMIBOLD else ShadowListUtilsTextStyle.WEIGHT_REGULAR)
    label.letterSpacing = -0.1f / 13f
    label.setTextColor(if (compact && isSelected) theme.colors.accent else theme.colors.label)
    label.layoutParams = (label.layoutParams as LayoutParams).apply { marginStart = if (compact) 10f.px else 0; marginEnd = marginStart }
    background = GradientDrawable().apply {
      setColor(if (isSelected) theme.colors.background else android.graphics.Color.TRANSPARENT)
      cornerRadius = (if (compact) 6f else 7f).pxf
    }
    alpha = ShadowListUtilsPressFeedback.alpha(isPressed && !isSelected)
    requestLayout(); invalidate()
  }
  override fun setPressed(pressed: Boolean) { super.setPressed(pressed); alpha = ShadowListUtilsPressFeedback.alpha(pressed && !isSelected) }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val unspecified = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
    label.measure(unspecified, unspecified)
    val inset = if (compact) 10f.px else 0
    val width = resolveSize(label.measuredWidth + inset * 2, widthMeasureSpec)
    val height = resolveSize((if (compact) 30f else 32f).px, heightMeasureSpec)
    label.measure(MeasureSpec.makeMeasureSpec((width - inset * 2).coerceAtLeast(0), MeasureSpec.EXACTLY), unspecified)
    divider.measure(MeasureSpec.makeMeasureSpec(1, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec((height - 14f.px).coerceAtLeast(0), MeasureSpec.EXACTLY))
    setMeasuredDimension(width, height)
  }
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    info.className = "android.app.ActionBar\$Tab"
    info.collectionItemInfo = AccessibilityNodeInfo.CollectionItemInfo.obtain(0, 1, index, 1, false, isSelected)
  }
}
