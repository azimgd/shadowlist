package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.ViewGroup
import com.shadowlist.kit.ShadowListKitTextLayout

/* Tool status is application-owned; disclosure is local presentation state. */
class ShadowListUtilsAssistantToolCallCard(context: Context, call: ShadowListUtilsAssistantToolCall, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : ViewGroup(context) {
  var call = call
    set(value) { if (field.key != value.key) expanded = false; field = value; requestLayout() }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; requestLayout() }
  var grouped = false
    set(value) { field = value; applyStyle() }
  var separated = false
    set(value) { field = value; separator.visibility = if (value) VISIBLE else GONE }
  internal var expanded = false
  internal var onLayoutChange: (() -> Unit)? = null
  private val disclosure = ShadowListUtilsAssistantDisclosure(context, theme)
  private val separator = View(context)
  init { addView(disclosure); addView(separator); separator.setBackgroundColor(theme.colors.separator); separator.visibility = GONE; applyStyle() }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val status = labels.toolStatus(call.status)
    val layout = layout(call, "${call.name}, $status", status, labels, expanded, width, theme)
    disclosure.status = call.status
    disclosure.show(layout.title, layout.body, expanded, ::toggle)
    disclosure.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(layout.heightPx, MeasureSpec.EXACTLY))
    setMeasuredDimension(width, resolveSize(layout.heightPx, heightMeasureSpec))
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    disclosure.layout(0, 0, width, height)
    val inset = 2 * theme.spacing.md.px + 20f.px
    if (layoutDirection == LAYOUT_DIRECTION_RTL) separator.layout(0, 0, (width - inset).coerceAtLeast(0), 1) else separator.layout(inset, 0, width, 1)
  }
  internal fun toggle() { expanded = !expanded; requestLayout(); onLayoutChange?.invoke() }
  private fun applyStyle() {
    disclosure.background = if (grouped) null else GradientDrawable().apply { setColor(theme.colors.elevated); cornerRadius = theme.radius.md.pxf }
  }
  internal data class Layout(val title: String, val text: String, val body: ShadowListUtilsAssistantDisclosureTextLayout, val widthPx: Int, val heightPx: Int)
  companion object {
    internal fun layout(call: ShadowListUtilsAssistantToolCall, title: String, status: String, labels: ShadowListUtilsAssistantLabels, expanded: Boolean, widthPx: Int, theme: ShadowListUtilsTheme): Layout {
      val output = when (val value = call.status) {
        is ShadowListUtilsAssistantToolStatus.Done -> value.output
        is ShadowListUtilsAssistantToolStatus.Failed -> value.output
        else -> null
      }
      val text = if (expanded) "${labels.toolInput}\n${call.input}" + (output?.takeIf { it.isNotEmpty() }?.let { "\n\n${labels.toolOutput}\n$it" } ?: "") else output?.takeIf { it.isNotEmpty() } ?: status
      val body = ShadowListUtilsAssistantDisclosureTextLayout(text, widthPx - 2 * theme.spacing.md.px, theme.typography.footnote, expanded, monospaced = expanded)
      return Layout(title, text, body, widthPx, 32f.px + body.height + 2 * theme.spacing.sm.px)
    }
  }
}
