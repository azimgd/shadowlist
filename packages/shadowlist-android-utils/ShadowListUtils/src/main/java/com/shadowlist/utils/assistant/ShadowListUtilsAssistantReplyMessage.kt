package com.shadowlist.utils

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.TextView
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.roundToInt

/*
 * A controlled reply. A live turn overrides the selected saved variant without changing either.
 * Presentation state changes notify its transcript, whose next row captures immutable layout input.
 */
class ShadowListUtilsAssistantReplyMessage(
  context: Context,
  message: ShadowListUtilsAssistantReply,
  turn: ShadowListUtilsAssistantTurn? = null,
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
) : ViewGroup(context) {
  var message = message.snapshot()
    set(value) {
      if (field.key != value.key) { thinkingExpanded = false; expandedToolKeys = emptySet(); copied = false; handler.removeCallbacks(resetCopied) }
      field = value.snapshot()
      requestLayout()
    }
  var turn = turn?.snapshot()
    set(value) { field = value?.snapshot(); requestLayout() }
  var busy = false
    set(value) { field = value; requestLayout() }
  var isLatest = false
    set(value) { field = value; requestLayout() }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; requestLayout() }
  var onCopy: ((String) -> Unit)? = null
  var onCopyCode: ((String) -> Unit)? = null
  var onShare: ((String) -> Unit)? = null
  var onRegenerate: ((String) -> Unit)? = null
  var onRetry: ((String) -> Unit)? = null
    set(value) { field = value; requestLayout() }
  var onSelectVariant: ((String, Int) -> Unit)? = null
  var onFeedback: ((String, ShadowListUtilsAssistantFeedback?) -> Unit)? = null
    set(value) { field = value; requestLayout() }
  var onFollowUp: ((String) -> Unit)? = null
  var onOpenLink: ((String) -> Unit)? = null
  internal var thinkingExpanded = false
  internal var expandedToolKeys = emptySet<String>()
  internal var onLayoutChange: (() -> Unit)? = null
  private val header = ShadowListKitTextView(context)
  private val sections = LinkedHashMap<String, View>()
  private val actionsScroll = HorizontalScrollView(context).apply { isHorizontalScrollBarEnabled = false }
  private val actions = FrameLayout(context)
  private val actionViews = LinkedHashMap<Action, View>()
  private val handler = Handler(Looper.getMainLooper())
  private val resetCopied = Runnable { copied = false; requestLayout() }
  private var copied = false
  private var frames: Layout? = null

  init {
    addView(header)
    actionsScroll.addView(actions)
    addView(actionsScroll)
  }

  internal data class TextSnapshot(
    val thinkingTitle: String,
    val toolTitles: Map<String, String>,
    val toolStatuses: Map<String, String>,
    val sourceLabels: Map<String, String>,
    val versionPosition: String,
  )

  internal sealed interface Section {
    val key: String
    val frame: Rect
    data class Typing(override val frame: Rect) : Section { override val key = "typing" }
    data class Text(override val key: String, val text: ShadowListKitTextLayout, override val frame: Rect, val error: Boolean) : Section
    data class Thinking(val title: String, val body: ShadowListUtilsAssistantDisclosureTextLayout, override val frame: Rect, val expanded: Boolean) : Section { override val key = "thinking" }
    data class Tool(val call: ShadowListUtilsAssistantToolCall, val title: String, val body: ShadowListUtilsAssistantDisclosureTextLayout,
                    override val frame: Rect, val expanded: Boolean) : Section { override val key = "tool-${call.key}" }
    data class Markdown(override val key: String, val text: String, val streaming: Boolean, override val frame: Rect) : Section
    data class Source(val source: ShadowListUtilsAssistantSource, val label: String, override val frame: Rect) : Section { override val key = "source-${source.key}" }
    data class FollowUp(val prompt: String, val index: Int, override val frame: Rect) : Section { override val key = "follow-$index" }
    data class Retry(override val frame: Rect) : Section { override val key = "retry" }
  }
  internal enum class Action { COPY, SHARE, REGENERATE, GOOD, BAD, PREVIOUS, NEXT, POSITION }
  internal data class ActionFrame(val action: Action, val label: String, val frame: Rect)
  internal data class Layout(val widthPx: Int, val heightPx: Int, val header: ShadowListKitTextLayout, val headerFrame: Rect,
                            val sections: List<Section>, val actions: List<ActionFrame>, val actionsFrame: Rect, val actionsWidth: Int, val streaming: Boolean)

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val layout = layout(message, turn, width, isLatest, thinkingExpanded, expandedToolKeys, onFeedback != null, onRetry != null, theme, labels,
      textSnapshot(message, turn, labels))
    frames = layout
    prepare(layout)
    setMeasuredDimension(width, resolveSize(layout.heightPx, heightMeasureSpec))
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) { frames?.let(::apply) }

  internal fun apply(layout: Layout) {
    prepare(layout)
    header.show(layout.header, theme.colors.secondaryLabel, layout.headerFrame)
    header.contentDescription = listOfNotNull(labels.assistantName, message.model).joinToString(" · ")
    for (section in layout.sections) {
      val view = sections.getValue(section.key)
      when (section) {
        is Section.Typing -> (view as ShadowListUtilsAssistantTypingIndicator).labels = labels
        is Section.Text -> (view as ShadowListKitTextView).apply {
          show(section.text, if (section.error) theme.colors.red else theme.colors.secondaryLabel, section.frame)
          contentDescription = section.text.layout.text.toString()
        }
        is Section.Thinking -> (view as ShadowListUtilsAssistantDisclosure).show(section.title, section.body, section.expanded) {
          thinkingExpanded = !thinkingExpanded; changedPresentation()
        }
        is Section.Tool -> (view as ShadowListUtilsAssistantDisclosure).apply { status = section.call.status }.show(section.title, section.body, section.expanded) {
          expandedToolKeys = if (section.call.key in expandedToolKeys) expandedToolKeys - section.call.key else expandedToolKeys + section.call.key
          changedPresentation()
        }
        is Section.Markdown -> Unit
        is Section.Source -> (view as Button).apply {
          text = section.source.title; contentDescription = section.label
          setOnClickListener { open(section.source.url) }
        }
        is Section.FollowUp -> (view as Button).apply { text = section.prompt; contentDescription = section.prompt; setOnClickListener { onFollowUp?.invoke(section.prompt) } }
        is Section.Retry -> (view as Button).apply { text = labels.retry; contentDescription = labels.retry; isEnabled = !busy; setOnClickListener { if (!busy) onRetry?.invoke(message.key) } }
      }
      view.place(section.frame)
    }
    actionsScroll.place(layout.actionsFrame)
    val actionParams = actions.layoutParams
    if (actionParams.width != layout.actionsWidth || actionParams.height != 32f.px) {
      actionParams.width = layout.actionsWidth; actionParams.height = 32f.px
      actions.layoutParams = actionParams
    }
    actions.measure(MeasureSpec.makeMeasureSpec(layout.actionsWidth, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(32f.px, MeasureSpec.EXACTLY))
    actions.layout(0, 0, layout.actionsWidth, 32f.px)
    for (action in layout.actions) {
      val view = actionViews.getValue(action.action)
      view.place(action.frame)
      view.visibility = if (layout.streaming) INVISIBLE else VISIBLE
      view.importantForAccessibility = if (layout.streaming) IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS else IMPORTANT_FOR_ACCESSIBILITY_AUTO
      if (action.action == Action.POSITION) (view as TextView).text = action.label
      else (view as ShadowListUtilsAssistantActionButton).apply {
        val nextLabel = if (action.action == Action.COPY && copied) labels.copied else action.label
        if (label != nextLabel) label = nextLabel
        val nextGlyph = glyph(action.action, copied)
        if (text.toString() != nextGlyph) text = nextGlyph
        isEnabled = !layout.streaming && !(busy && action.action == Action.REGENERATE) &&
          !(action.action == Action.PREVIOUS && message.variantIndex == 0) &&
          !(action.action == Action.NEXT && message.variantIndex == message.variants.lastIndex)
        isSelected = action.action == Action.GOOD && message.feedback == ShadowListUtilsAssistantFeedback.GOOD ||
          action.action == Action.BAD && message.feedback == ShadowListUtilsAssistantFeedback.BAD
      }
    }
  }

  private fun prepare(layout: Layout) {
    val used = layout.sections.map { it.key }.toSet()
    for (key in sections.keys.toList()) if (key !in used) sections.remove(key)?.let(::removeView)
    for (section in layout.sections) {
      val view = sections.getOrPut(section.key) {
        when (section) {
          is Section.Typing -> ShadowListUtilsAssistantTypingIndicator(context, theme)
          is Section.Text -> ShadowListKitTextView(context)
          is Section.Thinking -> ShadowListUtilsAssistantDisclosure(context, theme)
          is Section.Tool -> ShadowListUtilsAssistantDisclosure(context, theme).apply { background = GradientDrawable().apply { setColor(theme.colors.elevated); cornerRadius = theme.radius.md.pxf } }
          is Section.Markdown -> ShadowListUtilsAssistantMarkdown(context, theme = theme)
          else -> Button(context).apply { isAllCaps = false; setTextColor(theme.colors.label); textSize = theme.typography.footnote.fontSize }
        }.also(::addView)
      }
      if (section is Section.Markdown) (view as ShadowListUtilsAssistantMarkdown).apply {
        text = section.text
        streaming = section.streaming
        if (this.labels != this@ShadowListUtilsAssistantReplyMessage.labels) this.labels = this@ShadowListUtilsAssistantReplyMessage.labels
        onCopyCode = this@ShadowListUtilsAssistantReplyMessage.onCopyCode
        onOpenLink = this@ShadowListUtilsAssistantReplyMessage.onOpenLink
        measure(MeasureSpec.makeMeasureSpec(section.frame.width(), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(section.frame.height(), MeasureSpec.EXACTLY))
      }
    }
    val wanted = layout.actions.map { it.action }.toSet()
    for (key in actionViews.keys.toList()) if (key !in wanted) actionViews.remove(key)?.let(actions::removeView)
    for (action in layout.actions) actionViews.getOrPut(action.action) {
      if (action.action == Action.POSITION) TextView(context).apply { setTextColor(theme.colors.secondaryLabel); textSize = theme.typography.caption.fontSize; gravity = android.view.Gravity.CENTER }
      else ShadowListUtilsAssistantActionButton(context, action.label, theme = theme, onPress = { perform(action.action) }).apply { textSize = 18f }
    }.also { if (it.parent == null) actions.addView(it) }
  }

  private fun perform(action: Action) {
    val current = resolvedTurn(message, turn)
    when (action) {
      Action.COPY -> {
        onCopy?.invoke(current.content); copied = true; handler.removeCallbacks(resetCopied); handler.postDelayed(resetCopied, 1500); requestLayout()
      }
      Action.SHARE -> onShare?.invoke(current.content)
      Action.REGENERATE -> if (!busy) onRegenerate?.invoke(message.key)
      Action.GOOD -> onFeedback?.invoke(message.key, if (message.feedback == ShadowListUtilsAssistantFeedback.GOOD) null else ShadowListUtilsAssistantFeedback.GOOD)
      Action.BAD -> onFeedback?.invoke(message.key, if (message.feedback == ShadowListUtilsAssistantFeedback.BAD) null else ShadowListUtilsAssistantFeedback.BAD)
      Action.PREVIOUS -> if (message.variantIndex > 0) onSelectVariant?.invoke(message.key, message.variantIndex - 1)
      Action.NEXT -> if (message.variantIndex < message.variants.lastIndex) onSelectVariant?.invoke(message.key, message.variantIndex + 1)
      Action.POSITION -> Unit
    }
  }
  private fun changedPresentation() { requestLayout(); onLayoutChange?.invoke() }
  private fun open(url: String) {
    onOpenLink?.let { it(url); return }
    if (!ShadowListUtilsCore.isSafeUrl(url)) return
    try { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(ShadowListUtilsCore.trim(url))).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) } catch (_: ActivityNotFoundException) { }
  }
  override fun onDetachedFromWindow() { handler.removeCallbacks(resetCopied); copied = false; super.onDetachedFromWindow() }


  companion object {
    internal fun resolvedTurn(message: ShadowListUtilsAssistantReply, live: ShadowListUtilsAssistantTurn?): ShadowListUtilsAssistantTurn =
      live ?: message.variants.getOrNull(message.variantIndex) ?: ShadowListUtilsAssistantTurn(status = ShadowListUtilsAssistantTurnStatus.Done())

    /*
     * Resolve application formatters on the UI owner before a row is handed to layout prefetch.
     */
    internal fun textSnapshot(message: ShadowListUtilsAssistantReply, live: ShadowListUtilsAssistantTurn?, labels: ShadowListUtilsAssistantLabels): TextSnapshot {
      val turn = resolvedTurn(message, live)
      val active = turn.status == ShadowListUtilsAssistantTurnStatus.Streaming && (turn.thinkingMs ?: 0.0) == 0.0
      val seconds = if ((turn.thinkingMs ?: 0.0).isFinite()) maxOf(1, ((turn.thinkingMs ?: 0.0) / 1000).roundToInt()) else 1
      val done = turn.status as? ShadowListUtilsAssistantTurnStatus.Done
      val statuses = turn.toolCalls.associate { it.key to labels.toolStatus(it.status) }
      return TextSnapshot(if (turn.thinking.isNullOrEmpty()) "" else if (active) labels.thinking else labels.thoughtFor(seconds),
        turn.toolCalls.associate { it.key to "${it.name}, ${statuses[it.key]}" }, statuses,
        done?.sources?.mapIndexed { index, source -> source.key to labels.source(index + 1, source.title) }?.toMap().orEmpty(),
        if (message.variants.size > 1) labels.versionPosition(message.variantIndex + 1, message.variants.size) else "")
    }

    internal fun layout(message: ShadowListUtilsAssistantReply, live: ShadowListUtilsAssistantTurn?, widthPx: Int, isLatest: Boolean,
                        thinkingExpanded: Boolean, expandedToolKeys: Set<String>, hasFeedback: Boolean, hasRetry: Boolean,
                        theme: ShadowListUtilsTheme, labels: ShadowListUtilsAssistantLabels, texts: TextSnapshot): Layout {
      val turn = resolvedTurn(message, live)
      val streaming = turn.status == ShadowListUtilsAssistantTurnStatus.Streaming
      val spacing = theme.spacing
      val x = spacing.lg.px
      val inner = (widthPx - 2 * x).coerceAtLeast(1)
      val direction = ShadowListUtilsDirection(widthPx)
      val header = theme.typography.footnote.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(listOfNotNull(labels.assistantName, message.model).joinToString(" · "), inner)
      val headerFrame = frameOf(x, spacing.md.px, header)
      var y = headerFrame.bottom + spacing.md.px
      val sections = ArrayList<Section>()
      if (!turn.thinking.isNullOrEmpty()) {
        val layout = ShadowListUtilsAssistantThinking.layout(turn.thinking, texts.thinkingTitle, thinkingExpanded, inner, theme)
        val body = layout.body
        val frame = Rect(x, y, x + inner, y + layout.heightPx)
        sections.add(Section.Thinking(texts.thinkingTitle, body, direction.place(frame), thinkingExpanded)); y = frame.bottom + spacing.md.px
      }
      if (streaming && turn.thinking.isNullOrEmpty() && turn.content.isEmpty() && turn.toolCalls.isEmpty()) {
        val height = 7f.px + 2 * spacing.sm.px
        val frame = Rect(x, y, x + inner, y + height)
        sections.add(Section.Typing(direction.place(frame))); y = frame.bottom + spacing.md.px
      }
      val steps = ShadowListUtilsCore.stepsOf(turn.content, LongArray(turn.toolCalls.size) { turn.toolCalls[it].at?.toLong() ?: 0L })
      for ((index, step) in steps.withIndex()) when (step) {
        is ShadowListUtilsAssistantStep.Text -> {
          val running = streaming && index == steps.lastIndex
          val blocks = ShadowListUtilsCore.parseMarkdown(step.text)
          var height = blocks.sumOf { ShadowListUtilsAssistantMarkdownBlock.layout(it, inner, theme).heightPx + spacing.md.px }
          if (running) height += 6f.px + spacing.md.px
          height = (height - spacing.md.px).coerceAtLeast(0)
          sections.add(Section.Markdown("text-${step.at}", step.text, running, direction.place(Rect(x, y, x + inner, y + height))))
          y += height + spacing.md.px
        }
        is ShadowListUtilsAssistantStep.Calls -> {
          for (callIndex in step.callIndices) {
            val call = turn.toolCalls.getOrNull(callIndex) ?: continue
            val expanded = call.key in expandedToolKeys
            val layout = ShadowListUtilsAssistantToolCallCard.layout(call, texts.toolTitles[call.key].orEmpty(), texts.toolStatuses[call.key].orEmpty(), labels, expanded, inner, theme)
            val body = layout.body
            val frame = Rect(x, y, x + inner, y + layout.heightPx)
            sections.add(Section.Tool(call, texts.toolTitles[call.key].orEmpty(), body, direction.place(frame), expanded)); y = frame.bottom + spacing.xs.px
          }
          y += spacing.sm.px
        }
      }
      when (val status = turn.status) {
        ShadowListUtilsAssistantTurnStatus.Stopped -> {
          val text = theme.typography.footnote.layout(labels.responseStopped, inner)
          val frame = frameOf(x, y, text); sections.add(Section.Text("stopped", text, direction.place(frame), false)); y = frame.bottom + spacing.md.px
        }
        is ShadowListUtilsAssistantTurnStatus.Failed -> {
          val text = theme.typography.footnote.layout(status.error?.takeIf { it.isNotEmpty() } ?: labels.responseFailed, inner)
          val frame = frameOf(x, y, text); sections.add(Section.Text("failed", text, direction.place(frame), true)); y = frame.bottom + spacing.sm.px
          if (hasRetry) { sections.add(Section.Retry(direction.place(Rect(x, y, x + inner, y + 32f.px)))); y += 32f.px + spacing.md.px }
        }
        is ShadowListUtilsAssistantTurnStatus.Done -> for (source in status.sources) {
          val line = theme.typography.footnote.layout(source.title, (inner - 20f.px).coerceAtLeast(1))
          val height = maxOf(32f.px, line.height + 12f.px)
          sections.add(Section.Source(source, texts.sourceLabels[source.key].orEmpty(), direction.place(Rect(x, y, x + inner, y + height)))); y += height + spacing.xs.px
        }
        ShadowListUtilsAssistantTurnStatus.Streaming -> Unit
      }
      val actions = ArrayList<Pair<Action, String>>()
      if (turn.content.isNotEmpty()) actions.add(Action.COPY to labels.copy)
      actions.add(Action.REGENERATE to labels.regenerate)
      if (hasFeedback) { actions.add(Action.GOOD to labels.goodResponse); actions.add(Action.BAD to labels.badResponse) }
      if (turn.content.isNotEmpty()) actions.add(Action.SHARE to labels.share)
      if (message.variants.size > 1) { actions.add(Action.PREVIOUS to labels.previousVersion); actions.add(Action.POSITION to texts.versionPosition); actions.add(Action.NEXT to labels.nextVersion) }
      var actionX = 0
      val actionFrames = actions.map { (action, label) ->
        val width = if (action == Action.POSITION) maxOf(32f.px, theme.typography.caption.layout(label, inner).width + 8f.px) else 32f.px
        ActionFrame(action, label, Rect(actionX, 0, actionX + width, 32f.px)).also { actionX += width + spacing.xs.px }
      }
      val actionsFrame = direction.place(Rect(x, y, x + inner, y + 32f.px))
      y += 32f.px + spacing.md.px
      if (isLatest && turn.status is ShadowListUtilsAssistantTurnStatus.Done) for ((index, prompt) in turn.status.followUps.withIndex()) {
        val line = theme.typography.footnote.layout(prompt, (inner - 24f.px).coerceAtLeast(1))
        val height = maxOf(32f.px, line.height + 16f.px)
        sections.add(Section.FollowUp(prompt, index, direction.place(Rect(x, y, x + inner, y + height)))); y += height + spacing.sm.px
      }
      return Layout(widthPx, y + spacing.md.px, header, direction.place(headerFrame), sections, actionFrames, actionsFrame, actionX, streaming)
    }

    private fun glyph(action: Action, copied: Boolean): String = when (action) {
      Action.COPY -> if (copied) "✓" else "⧉"
      Action.SHARE -> "↗"
      Action.REGENERATE -> "↻"
      Action.GOOD -> "↑"
      Action.BAD -> "↓"
      Action.PREVIOUS -> "‹"
      Action.NEXT -> "›"
      Action.POSITION -> ""
    }
  }
}
