package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/* Shared disclosure surface used by standalone components and transcript sections. */
internal class ShadowListUtilsAssistantDisclosure(context: Context, private val theme: ShadowListUtilsTheme) : ViewGroup(context) {
  val title = Button(context).apply { isAllCaps = false; setTextColor(theme.colors.secondaryLabel); textSize = theme.typography.footnote.fontSize }
  private val progress = android.widget.ProgressBar(context).apply { importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO; visibility = GONE }
  var status: ShadowListUtilsAssistantToolStatus? = null
    set(value) { field = value; requestLayout() }
  val body = ShadowListUtilsAssistantDisclosureTextView(context)
  private var bodyLayout: ShadowListUtilsAssistantDisclosureTextLayout? = null
  private var expanded = false
  init {
    addView(title); addView(body); addView(progress)
    progress.indeterminateTintList = android.content.res.ColorStateList.valueOf(theme.colors.secondaryLabel)
    title.accessibilityDelegate = object : AccessibilityDelegate() {
      override fun onInitializeAccessibilityNodeInfo(host: View, info: android.view.accessibility.AccessibilityNodeInfo) {
        super.onInitializeAccessibilityNodeInfo(host, info)
        info.addAction(if (expanded) android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE else android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND)
      }
      override fun performAccessibilityAction(host: View, action: Int, arguments: android.os.Bundle?): Boolean {
        if (action == android.view.accessibility.AccessibilityNodeInfo.ACTION_EXPAND || action == android.view.accessibility.AccessibilityNodeInfo.ACTION_COLLAPSE) return title.performClick()
        return super.performAccessibilityAction(host, action, arguments)
      }
    }
  }
  fun show(label: String, text: ShadowListUtilsAssistantDisclosureTextLayout, expanded: Boolean, onToggle: () -> Unit) {
    val titleText = "${if (expanded) "▾" else "▸"} $label"
    if (title.text.toString() != titleText) title.text = titleText
    if (title.contentDescription?.toString() != label) title.contentDescription = label
    title.setOnClickListener { onToggle() }
    this.expanded = expanded
    body.contentDescription = text.layout.text.toString()
    bodyLayout = text
    text.apply(body, theme.colors.secondaryLabel)
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    progress.measure(MeasureSpec.makeMeasureSpec(16f.px, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(16f.px, MeasureSpec.EXACTLY))
    title.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(32f.px, MeasureSpec.EXACTLY))
    body.measure(MeasureSpec.makeMeasureSpec((bodyLayout?.width ?: width).coerceAtLeast(1), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(bodyLayout?.height ?: 0, MeasureSpec.EXACTLY))
    setMeasuredDimension(width, MeasureSpec.getSize(heightMeasureSpec))
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    title.layout(0, 0, right - left, 32f.px)
    val running = status == ShadowListUtilsAssistantToolStatus.Running
    progress.visibility = if (running) VISIBLE else GONE
    val marker = when (status) {
      is ShadowListUtilsAssistantToolStatus.Done -> "✓ "
      is ShadowListUtilsAssistantToolStatus.Failed -> "× "
      ShadowListUtilsAssistantToolStatus.Stopped -> "■ "
      else -> ""
    }
    val existing = title.contentDescription?.toString().orEmpty()
    val text = "${if (expanded) "▾" else "▸"} $marker$existing"
    val color = when (status) {
      is ShadowListUtilsAssistantToolStatus.Done -> theme.colors.green
      is ShadowListUtilsAssistantToolStatus.Failed -> theme.colors.red
      else -> theme.colors.tertiaryLabel
    }
    title.text = android.text.SpannableString(text).apply {
      if (marker.isNotEmpty()) setSpan(android.text.style.ForegroundColorSpan(color), 2, 2 + marker.length, android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    }
    title.setPadding(if (running && layoutDirection != LAYOUT_DIRECTION_RTL) 28f.px else 0, 0,
      if (running && layoutDirection == LAYOUT_DIRECTION_RTL) 28f.px else 0, 0)
    val start = if (layoutDirection == LAYOUT_DIRECTION_RTL) width - 24f.px else 8f.px
    progress.layout(start, 8f.px, start + 16f.px, 24f.px)
    body.layout(theme.spacing.md.px, 32f.px, theme.spacing.md.px + (bodyLayout?.width ?: 0), 32f.px + (bodyLayout?.height ?: 0))
  }
}
