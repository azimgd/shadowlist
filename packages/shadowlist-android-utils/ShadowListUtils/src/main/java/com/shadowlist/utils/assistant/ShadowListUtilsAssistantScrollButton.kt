package com.shadowlist.utils

import android.animation.ValueAnimator
import android.content.Context
import android.os.Build
import android.view.View
import android.widget.Button

/* Retains its layout slot while hidden, without touch or accessibility interception. */
class ShadowListUtilsAssistantScrollButton(context: Context, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : Button(context) {
  var visible = false
    set(value) { field = value; updateVisibility(isAttachedToWindow) }
  var onPress: (() -> Unit)? = null
    set(value) { field = value; isEnabled = value != null }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; contentDescription = value.scrollToLatest }

  init {
    text = "⌄"; contentDescription = labels.scrollToLatest
    minWidth = 0; minimumWidth = 0; minHeight = 0; minimumHeight = 0
    setPadding(0, 0, 0, 0)
    setTextColor(theme.colors.label)
    background = android.graphics.drawable.GradientDrawable().apply {
      setColor(theme.colors.background); cornerRadius = 18f.px.toFloat(); setStroke(1, theme.colors.separator)
    }
    setOnClickListener { if (visible) onPress?.invoke() }
    isEnabled = false
    updateVisibility(false)
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(resolveSize(36f.px, widthMeasureSpec), resolveSize(36f.px, heightMeasureSpec))
  }
  override fun onAttachedToWindow() { super.onAttachedToWindow(); updateVisibility(false) }
  override fun onDetachedFromWindow() { animate().cancel(); super.onDetachedFromWindow() }
  private fun updateVisibility(animated: Boolean) {
    isClickable = visible
    importantForAccessibility = if (visible) View.IMPORTANT_FOR_ACCESSIBILITY_YES else View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
    animate().cancel()
    val opacity = if (visible) 1f else 0f
    val scale = if (visible) 1f else 0.85f
    if (animated && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled())) {
      animate().alpha(opacity).scaleX(scale).scaleY(scale).setDuration(180).start()
    } else { alpha = opacity; scaleX = scale; scaleY = scale }
  }
}
