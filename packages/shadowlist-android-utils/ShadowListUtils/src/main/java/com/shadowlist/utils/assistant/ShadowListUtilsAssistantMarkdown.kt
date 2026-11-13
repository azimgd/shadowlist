package com.shadowlist.utils

import android.animation.ObjectAnimator
import android.animation.ValueAnimator
import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.view.View
import android.view.ViewGroup

/*
 * Markdown from the existing shared parser. Stable blocks keep their views as the last block grows.
 */
class ShadowListUtilsAssistantMarkdown(
  context: Context,
  text: String = "",
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
) : ViewGroup(context) {
  var text = text
    set(value) { if (field != value) { field = value; rebuild() } }
  var streaming = false
    set(value) { if (field != value) { field = value; updateMotion(); requestLayout() } }
  var onCopyCode: ((String) -> Unit)? = null
    set(value) { field = value; updateCallbacks() }
  var onOpenLink: ((String) -> Unit)? = null
    set(value) { field = value; updateCallbacks() }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { if (field != value) { field = value; rebuild(keepingViews = false) } }
  private var blocks = emptyList<ShadowListUtilsAssistantMarkdownBlock>()
  private val dot = View(context).apply {
    background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(theme.colors.secondaryLabel) }
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
  }
  private var pulse: ObjectAnimator? = null

  init { addView(dot); rebuild() }

  private fun rebuild(keepingViews: Boolean = true) {
    val previous = blocks.associateBy { it.block.key }
    blocks = ShadowListUtilsCore.parseMarkdown(text).map { block ->
      previous[block.key]?.takeIf { keepingViews && it.block.raw == block.raw }
        ?: ShadowListUtilsAssistantMarkdownBlock(context, block, theme, labels)
    }
    previous.values.filter { it !in blocks }.forEach(::removeView)
    blocks.filter { it.parent == null }.forEach(::addView)
    updateCallbacks()
    requestLayout()
  }

  private fun updateCallbacks() {
    blocks.forEach { it.onCopyCode = onCopyCode; it.onOpenLink = onOpenLink }
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    var height = 0
    for (block in blocks) {
      block.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
      height += block.measuredHeight + theme.spacing.md.px
    }
    if (streaming) height += 6f.px + theme.spacing.md.px
    setMeasuredDimension(width, resolveSize((height - theme.spacing.md.px).coerceAtLeast(0), heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    var y = 0
    for (block in blocks) { block.layout(0, y, right - left, y + block.measuredHeight); y += block.measuredHeight + theme.spacing.md.px }
    dot.visibility = if (streaming) VISIBLE else GONE
    dot.layout(0, y, 6f.px, y + 6f.px)
  }

  override fun onAttachedToWindow() { super.onAttachedToWindow(); updateMotion() }
  override fun onDetachedFromWindow() { pulse?.cancel(); pulse = null; super.onDetachedFromWindow() }

  private fun updateMotion() {
    pulse?.cancel()
    pulse = null
    if (!streaming || !isAttachedToWindow || (Build.VERSION.SDK_INT >= 26 && !ValueAnimator.areAnimatorsEnabled())) return
    pulse = ObjectAnimator.ofFloat(dot, "alpha", 0.35f, 1f).apply {
      duration = 800
      repeatMode = ValueAnimator.REVERSE
      repeatCount = ValueAnimator.INFINITE
      start()
    }
  }
}
