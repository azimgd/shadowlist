package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.floor

/*
 * A controlled user message. A hold exposes Copy and Edit; editing stays application-owned.
 */
class ShadowListUtilsAssistantUserMessage(
  context: Context,
  message: ShadowListUtilsAssistantPrompt,
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
) : ViewGroup(context) {
  var message = message.snapshot()
    set(value) { field = value.snapshot(); rebuild() }
  var busy = false
    set(value) { field = value; edit.isEnabled = !value }
  var onCopy: ((String) -> Unit)? = null
  var onEdit: ((String) -> Unit)? = null
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; rebuild() }
  private val bubble = FrameLayout(context)
  private val text = ShadowListKitTextView(context)
  private val copy = ShadowListUtilsAssistantActionButton(context, "Copy", showLabel = true, theme = theme)
  private val edit = ShadowListUtilsAssistantActionButton(context, "Edit", showLabel = true, theme = theme)
  private var attachments = emptyList<ShadowListUtilsAssistantAttachmentChip>()
  internal var actionsVisible = false
  internal var onLayoutChange: (() -> Unit)? = null
  private var frames: Layout? = null

  init {
    bubble.background = GradientDrawable().apply { setColor(theme.colors.elevated2); cornerRadius = theme.radius.xl.pxf }
    bubble.addView(text)
    bubble.isLongClickable = true
    bubble.setOnLongClickListener { toggleActions(); true }
    bubble.accessibilityDelegate = object : AccessibilityDelegate() {
      override fun onInitializeAccessibilityNodeInfo(host: View, info: AccessibilityNodeInfo) {
        super.onInitializeAccessibilityNodeInfo(host, info)
        info.className = "android.widget.Button"
        info.addAction(AccessibilityNodeInfo.AccessibilityAction(AccessibilityNodeInfo.ACTION_LONG_CLICK, labels.showActions))
      }
      override fun performAccessibilityAction(host: View, action: Int, args: Bundle?): Boolean {
        if (action == AccessibilityNodeInfo.ACTION_LONG_CLICK) { toggleActions(); return true }
        return super.performAccessibilityAction(host, action, args)
      }
    }
    listOf(bubble, copy, edit).forEach(::addView)
    copy.onPress = { onCopy?.invoke(this.message.text); hideActions() }
    edit.onPress = { if (!busy) { onEdit?.invoke(this.message.key); hideActions() } }
    rebuild()
  }

  private fun rebuild() {
    attachments.forEach(::removeView)
    attachments = message.attachments.map { ShadowListUtilsAssistantAttachmentChip(context, it, theme) }
    attachments.forEach(::addView)
    bubble.contentDescription = message.text
    copy.label = labels.copy
    edit.label = labels.edit
    requestLayout()
  }

  internal data class Layout(
    val widthPx: Int, val heightPx: Int, val text: ShadowListKitTextLayout,
    val bubbleFrame: Rect, val textFrame: Rect, val attachmentFrames: List<Rect>, val copyFrame: Rect, val editFrame: Rect,
  )

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val layout = layout(message, width, actionsVisible, theme, labels)
    frames = layout
    setMeasuredDimension(width, resolveSize(layout.heightPx, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) { frames?.let(::apply) }

  internal fun apply(layout: Layout) {
    bubble.place(layout.bubbleFrame)
    bubble.visibility = if (message.text.isEmpty()) GONE else VISIBLE
    text.show(layout.text, theme.colors.label, layout.textFrame)
    attachments.forEachIndexed { index, attachment -> attachment.place(layout.attachmentFrames[index]) }
    copy.place(layout.copyFrame)
    edit.place(layout.editFrame)
    copy.visibility = if (actionsVisible) VISIBLE else GONE
    edit.visibility = if (actionsVisible) VISIBLE else GONE
  }

  internal fun toggleActions() { actionsVisible = !actionsVisible; changedLayout() }
  private fun hideActions() { actionsVisible = false; changedLayout() }
  private fun changedLayout() { requestLayout(); onLayoutChange?.invoke() }

  companion object {
    internal fun layout(message: ShadowListUtilsAssistantPrompt, widthPx: Int, actionsVisible: Boolean,
                        theme: ShadowListUtilsTheme, labels: ShadowListUtilsAssistantLabels): Layout {
      val spacing = theme.spacing
      val body = theme.typography.body
      val metrics = DoubleArray(ShadowListUtilsCore.CHAT_METRIC_SLOTS)
      metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_XXS] = spacing.xxs.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_XS] = spacing.xs.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_SM] = spacing.sm.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_MD] = spacing.md.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_SPACING_LG] = spacing.lg.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_FONT_SIZE] = body.fontSize.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_LINE_HEIGHT] = body.scaledLineHeightPx().toFloat().dp.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_BODY_LETTER_SPACING] = body.letterSpacing.toDouble()
      metrics[ShadowListUtilsCore.CHAT_METRIC_CAPTION_LINE_HEIGHT] = theme.typography.caption.scaledLineHeightPx().toFloat().dp.toDouble()
      val spec = DoubleArray(ShadowListUtilsCore.CHAT_OUT_SLOTS)
      ShadowListUtilsCore.assistantUserMessageSizeSpec(true, false, metrics, spec)
      val textWidth = floor(spec[ShadowListUtilsCore.CHAT_OUT_WIDTH_FRACTION] * widthPx - spec[ShadowListUtilsCore.CHAT_OUT_INSET_WIDTH].toFloat().pxf).toInt().coerceAtLeast(1)
      val line = body.layout(message.text, textWidth)
      val paddingX = ShadowListUtilsCore.ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL_DP.px
      val paddingY = ShadowListUtilsCore.ASSISTANT_USER_BUBBLE_PADDING_VERTICAL_DP.px
      val direction = ShadowListUtilsDirection(widthPx)
      var y = spacing.sm.px
      val attachmentFrames = ArrayList<Rect>()
      var lineWidth = 0
      var lineHeight = 0
      val pending = ArrayList<Rect>()
      fun finishLine() {
        val offset = widthPx - spacing.lg.px - lineWidth
        attachmentFrames.addAll(pending.map { direction.place(Rect(it).apply { this.offset(offset, 0) }) })
        pending.clear()
        y += lineHeight + spacing.sm.px
        lineWidth = 0
        lineHeight = 0
      }
      for (attachment in message.attachments) {
        val size = ShadowListUtilsAssistantAttachmentChip.layout(attachment, widthPx - 2 * spacing.lg.px, false, theme)
        if (pending.isNotEmpty() && lineWidth + spacing.sm.px + size.widthPx > widthPx - 2 * spacing.lg.px) finishLine()
        if (pending.isNotEmpty()) lineWidth += spacing.sm.px
        pending.add(Rect(lineWidth, y, lineWidth + size.widthPx, y + size.heightPx))
        lineWidth += size.widthPx
        lineHeight = maxOf(lineHeight, size.heightPx)
      }
      if (pending.isNotEmpty()) finishLine()
      val bubbleWidth = line.width + 2 * paddingX
      val bubble = if (message.text.isEmpty()) Rect() else Rect(widthPx - spacing.lg.px - bubbleWidth, y,
        widthPx - spacing.lg.px, y + line.height + 2 * paddingY)
      if (message.text.isNotEmpty()) y = bubble.bottom
      fun actionWidth(label: String): Int = maxOf(32f.px, theme.typography.footnote.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(label, widthPx).width + 38f.px + spacing.xs.px)
      val copyWidth = actionWidth(labels.copy)
      val editWidth = actionWidth(labels.edit)
      val actionsY = y + spacing.xs.px
      val editFrame = Rect(widthPx - spacing.lg.px - editWidth, actionsY, widthPx - spacing.lg.px, actionsY + 32f.px)
      val copyFrame = Rect(editFrame.left - spacing.xs.px - copyWidth, actionsY, editFrame.left - spacing.xs.px, actionsY + 32f.px)
      if (actionsVisible) y = actionsY + 32f.px
      return Layout(widthPx, y + spacing.sm.px, line, direction.place(bubble), Rect(paddingX, paddingY, paddingX + line.width, paddingY + line.height),
        attachmentFrames, direction.place(copyFrame), direction.place(editFrame))
    }
  }
}
