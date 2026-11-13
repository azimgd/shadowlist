package com.shadowlist.utils

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.graphics.Rect
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.TextPaint
import android.text.style.ForegroundColorSpan
import android.text.style.MetricAffectingSpan
import android.text.style.UnderlineSpan
import android.text.style.URLSpan
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.TextView
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/*
 * One shared-parser block. The kit's same StaticLayout measures, paints and locates links.
 */
internal class ShadowListUtilsAssistantMarkdownBlock(
  context: Context,
  val block: ShadowListUtilsMarkdownBlock,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsAssistantLabels,
) : ViewGroup(context) {
  var onCopyCode: ((String) -> Unit)? = null
  var onOpenLink: ((String) -> Unit)? = null
  private val scroll = HorizontalScrollView(context).apply { isHorizontalScrollBarEnabled = false }
  private val content = FrameLayout(context)
  private val border = View(context)
  private val language = TextView(context)
  internal val copyButton = Button(context)
  private val handler = Handler(Looper.getMainLooper())
  private val resetCopied = Runnable { showCopyLabel(false) }
  private val textViews = ArrayList<ShadowListKitTextView>()
  private val texts = ArrayList<CharSequence>()
  private val styles = ArrayList<ShadowListUtilsTextStyle>()
  private val layouts = ArrayList<ShadowListKitTextLayout>()
  private val frames = ArrayList<Rect>()
  private var contentWidth = 0
  private var contentHeight = 0
  private var topPx = 0
  private var quoteInsetPx = 0

  init {
    scroll.addView(content)
    addView(scroll)
    border.setBackgroundColor(theme.colors.separator)
    addView(border)
    if (block.type == ShadowListUtilsMarkdownBlock.Type.CODE) {
      background = GradientDrawable().apply { setColor(theme.colors.elevated); cornerRadius = theme.radius.sm.pxf }
      language.text = block.language.ifEmpty { labels.codeLanguageFallback }
      language.setTextColor(theme.colors.secondaryLabel)
      language.setTextSize(TypedValue.COMPLEX_UNIT_SP, theme.typography.caption.fontSize)
      addView(language)
      showCopyLabel(false)
      copyButton.visibility = if (block.closed) VISIBLE else GONE
      copyButton.setOnClickListener {
        if (block.closed) {
          onCopyCode?.invoke(block.code)
          showCopyLabel(true)
          handler.removeCallbacks(resetCopied)
          handler.postDelayed(resetCopied, 1500)
        }
      }
      addView(copyButton)
    }
    for ((text, style) in makeTexts(block, theme)) append(text, style)
  }


  private fun append(text: CharSequence, style: ShadowListUtilsTextStyle) {
    val view = ShadowListKitTextView(context)
    view.contentDescription = text.toString()
    val spanned = text as? Spanned
    val links = spanned?.getSpans(0, text.length, URLSpan::class.java) ?: emptyArray()
    view.accessibilityDelegate = object : AccessibilityDelegate() {
      override fun onInitializeAccessibilityNodeInfo(host: View, info: AccessibilityNodeInfo) {
        super.onInitializeAccessibilityNodeInfo(host, info)
        info.className = "android.widget.TextView"
        if (android.os.Build.VERSION.SDK_INT >= 28) info.isHeading = block.type == ShadowListUtilsMarkdownBlock.Type.HEADING
        for ((index, link) in links.withIndex()) info.addAction(AccessibilityNodeInfo.AccessibilityAction(
          0x01000000 + index, text.subSequence(spanned?.getSpanStart(link) ?: 0, spanned?.getSpanEnd(link) ?: 0)))
      }
      override fun performAccessibilityAction(host: View, action: Int, args: Bundle?): Boolean {
        val link = links.getOrNull(action - 0x01000000)
        return if (link != null) open(link.url) else super.performAccessibilityAction(host, action, args)
      }
    }
    view.setOnTouchListener { _, event ->
      if (event.actionMasked != MotionEvent.ACTION_UP) return@setOnTouchListener links.isNotEmpty()
      val index = textViews.indexOf(view)
      val layout = layouts.getOrNull(index)?.layout ?: return@setOnTouchListener false
      if (event.y < 0 || event.y >= layout.height) return@setOnTouchListener false
      val line = layout.getLineForVertical(event.y.toInt())
      if (event.x < layout.getLineLeft(line) || event.x > layout.getLineRight(line)) return@setOnTouchListener false
      val at = layout.getOffsetForHorizontal(line, event.x)
      val link = (text as? Spanned)?.getSpans(at, at, URLSpan::class.java)?.firstOrNull()
      if (link != null) open(link.url) else false
    }
    content.addView(view)
    textViews.add(view)
    texts.add(text)
    styles.add(style)
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val layout = layout(block, width, theme)
    topPx = layout.topPx
    quoteInsetPx = layout.quoteInsetPx
    contentWidth = layout.contentWidthPx
    contentHeight = layout.contentHeightPx
    frames.clear()
    frames.addAll(layout.frames)
    layouts.clear()
    layouts.addAll(layout.layouts)
    content.layoutParams = FrameLayout.LayoutParams(maxOf(contentWidth, width - quoteInsetPx), contentHeight)
    scroll.measure(MeasureSpec.makeMeasureSpec((width - quoteInsetPx).coerceAtLeast(1), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(contentHeight, MeasureSpec.EXACTLY))
    setMeasuredDimension(width, resolveSize(layout.heightPx, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    if (block.type == ShadowListUtilsMarkdownBlock.Type.RULE) { border.layout(0, 0, right - left, 1); return }
    scroll.layout(quoteInsetPx, topPx, right - left, topPx + contentHeight)
    for (index in textViews.indices) textViews[index].show(layouts[index], theme.colors.label, frames[index])
    border.visibility = if (block.type == ShadowListUtilsMarkdownBlock.Type.QUOTE) VISIBLE else GONE
    border.layout(0, 0, 3f.px, contentHeight)
    language.layout(theme.spacing.md.px, 0, (right - left - 110f.px).coerceAtLeast(0), topPx)
    copyButton.layout((right - left - 100f.px).coerceAtLeast(0), 0, right - left - theme.spacing.md.px, topPx)
  }

  private fun showCopyLabel(copied: Boolean) {
    copyButton.text = if (copied) labels.copied else labels.copy
    copyButton.contentDescription = if (copied) labels.copied else labels.copyCode
  }

  override fun onDetachedFromWindow() {
    handler.removeCallbacks(resetCopied)
    showCopyLabel(false)
    super.onDetachedFromWindow()
  }

  internal fun open(href: String): Boolean {
    onOpenLink?.let { it(href); return true }
    if (!ShadowListUtilsCore.isSafeUrl(href)) return false
    return try {
      context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(ShadowListUtilsCore.trim(href))).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
      true
    } catch (_: ActivityNotFoundException) { false }
  }
  internal data class Layout(
    val widthPx: Int, val heightPx: Int,
    val texts: List<CharSequence>, val layouts: List<ShadowListKitTextLayout>, val frames: List<Rect>,
    val contentWidthPx: Int, val contentHeightPx: Int, val topPx: Int, val quoteInsetPx: Int,
  )

  companion object {
  private fun font(typeface: Typeface) = object : MetricAffectingSpan() {
    override fun updateDrawState(paint: TextPaint) = updateMeasureState(paint)
    override fun updateMeasureState(paint: TextPaint) { paint.typeface = typeface }
  }

    private fun makeTexts(block: ShadowListUtilsMarkdownBlock, theme: ShadowListUtilsTheme): List<Pair<CharSequence, ShadowListUtilsTextStyle>> {
      val result = ArrayList<Pair<CharSequence, ShadowListUtilsTextStyle>>()
      fun append(text: CharSequence, style: ShadowListUtilsTextStyle) { result.add(text to style) }
      if (block.type == ShadowListUtilsMarkdownBlock.Type.CODE) {
      val code = SpannableString(block.code.split("\n").joinToString("\n") { it.ifEmpty { "\u00a0" } })
      code.setSpan(font(Typeface.MONOSPACE), 0, code.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
      append(code, ShadowListUtilsTextStyle(14f, ShadowListUtilsFontWeight.REGULAR, 20f, 0f))
    } else if (block.type != ShadowListUtilsMarkdownBlock.Type.RULE) {
      val groups = ArrayList<Array<ShadowListUtilsMarkdownInline>>()
      if (block.type == ShadowListUtilsMarkdownBlock.Type.TABLE) {
        val columns = block.rowCellCounts.firstOrNull() ?: 0
        var at = 0
        for (count in block.rowCellCounts) {
          repeat(columns) { column -> groups.add(if (column < count) block.runs[at + column] else emptyArray()) }
          at += count
        }
      } else groups.addAll(block.runs)
      for ((index, runs) in groups.withIndex()) {
        val style = if (block.type == ShadowListUtilsMarkdownBlock.Type.HEADING) {
          when (block.level) { 1 -> theme.typography.title2; 2 -> theme.typography.title3; else -> theme.typography.headline }
        } else if (block.type == ShadowListUtilsMarkdownBlock.Type.TABLE) theme.typography.subhead else theme.typography.body
        val text = SpannableStringBuilder()
        if (block.type == ShadowListUtilsMarkdownBlock.Type.LIST) {
          val number = block.start + index
          val marker = if (block.ordered) "${if (number % 1.0 == 0.0) String.format(java.util.Locale.ROOT, "%.0f", number) else number}. " else "• "
          text.append(marker)
          text.setSpan(ForegroundColorSpan(theme.colors.secondaryLabel), 0, text.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
        val headerCell = block.type == ShadowListUtilsMarkdownBlock.Type.TABLE && index < (block.rowCellCounts.firstOrNull() ?: 0)
        for (run in runs) {
          val start = text.length
          text.append(run.text)
          val end = text.length
          if (start == end) continue
          text.setSpan(ForegroundColorSpan(if (block.type == ShadowListUtilsMarkdownBlock.Type.QUOTE) theme.colors.secondaryLabel else theme.colors.label), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
          if (run.style == ShadowListUtilsMarkdownInline.Style.BOLD || headerCell) text.setSpan(
            font(ShadowListUtilsTextStyle.typeface(ShadowListUtilsTextStyle.WEIGHT_SEMIBOLD)), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
          if (run.style == ShadowListUtilsMarkdownInline.Style.ITALIC) text.setSpan(font(Typeface.create(Typeface.DEFAULT, Typeface.ITALIC)), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
          if (run.style == ShadowListUtilsMarkdownInline.Style.CODE) text.setSpan(font(Typeface.MONOSPACE), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
          if (run.style == ShadowListUtilsMarkdownInline.Style.LINK) {
            text.setSpan(ForegroundColorSpan(theme.colors.accent), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            text.setSpan(UnderlineSpan(), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            if (run.href != null) text.setSpan(URLSpan(run.href), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
          }
        }
        append(text, style)
      }
    }
      return result
    }

    internal fun layout(block: ShadowListUtilsMarkdownBlock, width: Int, theme: ShadowListUtilsTheme): Layout {
      val content = makeTexts(block, theme)
      val padding = if (block.type == ShadowListUtilsMarkdownBlock.Type.CODE || block.type == ShadowListUtilsMarkdownBlock.Type.TABLE) theme.spacing.md.px else 0
      val quoteInset = if (block.type == ShadowListUtilsMarkdownBlock.Type.QUOTE) theme.spacing.md.px else 0
      val top = if (block.type == ShadowListUtilsMarkdownBlock.Type.CODE) 40f.px else padding
      val columns = if (block.type == ShadowListUtilsMarkdownBlock.Type.TABLE) (block.rowCellCounts.firstOrNull() ?: 1).coerceAtLeast(1) else 1
      val textWidth = when (block.type) {
        ShadowListUtilsMarkdownBlock.Type.TABLE -> 140f.px - 2 * padding
        ShadowListUtilsMarkdownBlock.Type.CODE -> 100000f.px
        else -> (width - quoteInset).coerceAtLeast(1)
      }
      var x = padding
      var y = padding
      var rowHeight = 0
      var contentWidth = 0
      val frames = ArrayList<Rect>()
      val layouts = ArrayList<ShadowListKitTextLayout>()
      for ((index, entry) in content.withIndex()) {
        if (block.type == ShadowListUtilsMarkdownBlock.Type.TABLE && index > 0 && index % columns == 0) {
          x = padding
          y += rowHeight + 2 * padding
          rowHeight = 0
        }
        val layout = entry.second.layout(entry.first, textWidth, 0, scaled = true)
        layouts.add(layout)
        val frame = Rect(x, y, x + layout.width.coerceAtLeast(1), y + layout.height)
        frames.add(frame)
        rowHeight = maxOf(rowHeight, frame.height())
        contentWidth = maxOf(contentWidth, frame.right + padding)
        if (block.type == ShadowListUtilsMarkdownBlock.Type.TABLE) x += 140f.px
        else y = frame.bottom + if (block.type == ShadowListUtilsMarkdownBlock.Type.LIST) theme.spacing.xs.px else 0
      }
      val height = if (block.type == ShadowListUtilsMarkdownBlock.Type.TABLE) y + rowHeight + 2 * padding else (y - if (block.type == ShadowListUtilsMarkdownBlock.Type.LIST) theme.spacing.xs.px else 0).coerceAtLeast(0)
      return Layout(width, if (block.type == ShadowListUtilsMarkdownBlock.Type.RULE) 1 else top + height + padding,
        content.map { it.first }, layouts, frames, contentWidth, height, top, quoteInset)
    }
  }

}
