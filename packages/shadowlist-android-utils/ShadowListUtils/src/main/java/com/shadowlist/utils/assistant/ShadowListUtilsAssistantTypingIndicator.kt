package com.shadowlist.utils

import android.animation.ObjectAnimator
import android.animation.ValueAnimator
import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo

/* Three composited opacity pulses with lifecycle-owned animators. */
class ShadowListUtilsAssistantTypingIndicator(context: Context, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : ViewGroup(context) {
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; contentDescription = value.typing }
  private val dots = List(3) { View(context).apply {
    background = GradientDrawable().apply { setColor(theme.colors.secondaryLabel); cornerRadius = 3.5f.px.toFloat() }
  } }
  private var ready = false
  private val pulses = mutableListOf<ObjectAnimator>()
  init { dots.forEach(::addView); isFocusable = true; importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES; contentDescription = labels.typing; ready = true }
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info); info.className = "android.widget.ProgressBar"
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(resolveSize(21f.px + 2 * theme.spacing.xs.px, widthMeasureSpec), resolveSize(7f.px + 2 * theme.spacing.sm.px, heightMeasureSpec))
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    dots.forEachIndexed { index, dot ->
      val x = index * (7f.px + theme.spacing.xs.px)
      val start = if (layoutDirection == LAYOUT_DIRECTION_RTL) width - x - 7f.px else x
      dot.layout(start, theme.spacing.sm.px, start + 7f.px, theme.spacing.sm.px + 7f.px)
    }
  }
  override fun onVisibilityChanged(changedView: View, visibility: Int) {
    super.onVisibilityChanged(changedView, visibility)
    if (ready) { if (isShown && isAttachedToWindow) startPulse() else stopPulse() }
  }
  override fun onAttachedToWindow() { super.onAttachedToWindow(); startPulse() }
  override fun onDetachedFromWindow() { stopPulse(); super.onDetachedFromWindow() }
  private fun stopPulse() { pulses.forEach { it.cancel() }; pulses.clear(); dots.forEach { it.alpha = 1f } }
  private fun startPulse() {
    stopPulse()
    if (!isShown || !isAttachedToWindow) return
    if (Build.VERSION.SDK_INT >= 26 && !ValueAnimator.areAnimatorsEnabled()) return
    dots.forEachIndexed { index, dot ->
      pulses.add(ObjectAnimator.ofFloat(dot, "alpha", 0.25f, 1f).apply {
        duration = 600; startDelay = index * 160L; repeatCount = ValueAnimator.INFINITE; repeatMode = ValueAnimator.REVERSE; start()
      })
    }
  }
}
