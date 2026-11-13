package com.shadowlist.utils

import android.animation.ObjectAnimator
import android.animation.ValueAnimator
import android.content.Context
import android.os.Build
import android.view.ViewGroup
import com.shadowlist.kit.ShadowListKitTextLayout
import kotlin.math.roundToInt

/* Local disclosure state, independent of the application-owned live turn. */
class ShadowListUtilsAssistantThinking(context: Context, thinking: String, active: Boolean, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : ViewGroup(context) {
  var thinking = thinking
    set(value) { field = value; requestLayout() }
  var thinkingMs = 0.0
    set(value) { field = value; requestLayout() }
  var active = active
    set(value) { field = value; requestLayout(); updatePulse() }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; requestLayout() }
  internal var expanded = false
  internal var onLayoutChange: (() -> Unit)? = null
  private val disclosure = ShadowListUtilsAssistantDisclosure(context, theme)
  private var pulse: ObjectAnimator? = null
  private var frames: Layout? = null
  init { addView(disclosure); disclosure.background = null }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val layout = layout(thinking, title(thinkingMs, active, labels), expanded, width, theme)
    frames = layout
    disclosure.show(layout.title, layout.body, expanded, ::toggle)
    disclosure.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(layout.heightPx, MeasureSpec.EXACTLY))
    setMeasuredDimension(width, resolveSize(layout.heightPx, heightMeasureSpec))
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) { disclosure.layout(0, 0, width, height); updatePulse() }
  override fun onAttachedToWindow() { super.onAttachedToWindow(); updatePulse() }
  override fun onDetachedFromWindow() { pulse?.cancel(); pulse = null; super.onDetachedFromWindow() }
  internal fun toggle() { expanded = !expanded; requestLayout(); onLayoutChange?.invoke() }
  private fun updatePulse() {
    val shouldAnimate = active && isAttachedToWindow && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled())
    if (shouldAnimate && pulse != null) return
    pulse?.cancel(); pulse = null; disclosure.title.alpha = 1f
    if (shouldAnimate) pulse = ObjectAnimator.ofFloat(disclosure.title, "alpha", 0.25f, 1f).apply {
      duration = 600; repeatMode = ValueAnimator.REVERSE; repeatCount = ValueAnimator.INFINITE; start()
    }
  }
  internal data class Layout(val title: String, val text: String, val body: ShadowListUtilsAssistantDisclosureTextLayout, val widthPx: Int, val heightPx: Int)
  companion object {
    internal fun title(thinkingMs: Double, active: Boolean, labels: ShadowListUtilsAssistantLabels): String {
      if (active) return labels.thinking
      val seconds = if (!thinkingMs.isFinite()) 1 else (thinkingMs / 1000).roundToInt().coerceAtLeast(1)
      return labels.thoughtFor(seconds)
    }
    internal fun layout(thinking: String, title: String, expanded: Boolean, widthPx: Int, theme: ShadowListUtilsTheme): Layout {
      val text = if (expanded) thinking else thinking.takeLast(160)
      val body = ShadowListUtilsAssistantDisclosureTextLayout(text, widthPx - theme.spacing.md.px, theme.typography.footnote, expanded)
      return Layout(title, text, body, widthPx, 32f.px + body.height + theme.spacing.sm.px)
    }
  }
}
