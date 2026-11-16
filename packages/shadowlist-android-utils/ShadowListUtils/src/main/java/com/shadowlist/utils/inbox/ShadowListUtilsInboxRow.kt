package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.max

/*
 * A mail row: an avatar, the sender, the message's age, the subject and a preview of two lines.
 * An unread dot and a flag mark its state. While editing it shows a selection circle in place
 * of the dot. The row highlights while pressed, and while selected in a list that is editing.
 * The age is worked out once, when the row is made.
 */
data class ShadowListUtilsInboxRow(
  val item: ShadowListUtilsInboxMessage,
  val editing: Boolean,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsInboxLabels,
) : ShadowListUtilsRow {
  /*
   * The message's age in words.
   */
  val time: String = labels.time.format(item.receivedAtMs, System.currentTimeMillis().toDouble())

  override val key: String get() = item.key
  override val contentVersion: Int = 31 * (31 * item.hashCode() + editing.hashCode()) + time.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsInboxRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val typography = theme.typography
    val direction = ShadowListUtilsDirection(widthPx)
    val top = spacing.md.px
    val gutter = GUTTER_WIDTH_DP.px
    val avatarSize = AVATAR_SIZE_DP.px
    val avatarFrame = Rect(gutter, top, gutter + avatarSize, top + avatarSize)
    val bodyX = (GUTTER_WIDTH_DP + AVATAR_SIZE_DP + spacing.md).px
    val bodyWidth = (widthPx - bodyX - spacing.lg.px).coerceAtLeast(1)

    // The age keeps its width at the trailing edge, the flag sits before it and the sender takes the rest.
    val time = typography.footnote.layout(this.time, bodyWidth, 1)
    val flagSize = FLAG_SIZE_DP.px
    val flagWidth = if (item.flagged) flagSize + 2 * spacing.xs.px else 0
    val senderStyle = if (item.read) typography.body else typography.body.with(ShadowListUtilsFontWeight.SEMIBOLD)
    val sender = senderStyle.layout(item.sender, max(1, bodyWidth - time.width - flagWidth), 1)
    val titleHeight = typography.body.scaledLineHeightPx()
    val timeFrame = frameOf(bodyX + bodyWidth - time.width, top + (titleHeight - typography.footnote.scaledLineHeightPx()) / 2, time)
    val subject = typography.subhead.layout(item.subject, bodyWidth, 1)
    val preview = typography.subhead.layout(item.preview, bodyWidth, PREVIEW_LINES)
    val subjectFrame = frameOf(bodyX, top + titleHeight, subject)
    val previewFrame = frameOf(bodyX, subjectFrame.top + typography.subhead.scaledLineHeightPx(), preview)
    val dotSize = UNREAD_DOT_SIZE_DP.px
    val dotLeft = (gutter - dotSize) / 2
    val dotTop = top + UNREAD_DOT_TOP_DP.px
    val unreadDotFrame = Rect(dotLeft, dotTop, dotLeft + dotSize, dotTop + dotSize)
    val checkSize = CHECK_SIZE_DP.px
    val checkLeft = (gutter - checkSize) / 2
    val checkTop = top + CHECK_TOP_DP.px
    val checkFrame = Rect(checkLeft, checkTop, checkLeft + checkSize, checkTop + checkSize)
    val flagLeft = timeFrame.left - spacing.xs.px - flagSize
    val flagTop = top + (titleHeight - flagSize) / 2
    val flagFrame = Rect(flagLeft, flagTop, flagLeft + flagSize, flagTop + flagSize)
    return ShadowListUtilsInboxRowLayout(
      widthPx, max(avatarFrame.bottom, previewFrame.bottom) + spacing.md.px, direction.place(unreadDotFrame),
      direction.place(checkFrame), direction.place(avatarFrame), sender, direction.place(frameOf(bodyX, top, sender)),
      direction.place(flagFrame), time, direction.place(timeFrame), subject, direction.place(subjectFrame), preview,
      direction.place(previewFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsInboxRow"
    const val AVATAR_SIZE_DP = 40f
    const val GUTTER_WIDTH_DP = 28f
    const val UNREAD_DOT_SIZE_DP = 10f
    const val CHECK_SIZE_DP = 22f
    const val FLAG_SIZE_DP = 8f
    private const val UNREAD_DOT_TOP_DP = 16f
    private const val CHECK_TOP_DP = 9f
    private const val PREVIEW_LINES = 2
  }
}

class ShadowListUtilsInboxRowLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val unreadDotFrame: Rect,
  val checkFrame: Rect,
  val avatarFrame: Rect,
  val sender: ShadowListKitTextLayout,
  val senderFrame: Rect,
  val flagFrame: Rect,
  val time: ShadowListKitTextLayout,
  val timeFrame: Rect,
  val subject: ShadowListKitTextLayout,
  val subjectFrame: Rect,
  val preview: ShadowListKitTextLayout,
  val previewFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsInboxRowView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsInboxRow.REUSE_IDENTIFIER) {
  private val unreadDot = View(context)
  private val check = View(context)
  private val checkIcon = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.CHECK)
  private val avatar = ShadowListUtilsAvatar(context, ShadowListUtilsInboxRow.AVATAR_SIZE_DP, ShadowListUtilsTheme.LIGHT)
  private val sender = ShadowListKitTextView(context)
  private val flag = View(context)
  private val time = ShadowListKitTextView(context)
  private val subject = ShadowListKitTextView(context)
  private val preview = ShadowListKitTextView(context)
  private val unreadDotShape = GradientDrawable().apply { shape = GradientDrawable.OVAL }
  private val checkShape = GradientDrawable().apply { shape = GradientDrawable.OVAL }
  private val flagShape = GradientDrawable()

  init {
    unreadDot.background = unreadDotShape
    check.background = checkShape
    flag.background = flagShape
    flagShape.cornerRadius = FLAG_RADIUS_DP.pxf
    for (view in listOf(unreadDot, check, checkIcon, avatar, sender, flag, time, subject, preview)) addView(view)
  }

  /*
   * The list tells its cells when it edits. A row made with editing shows the circle too, for a
   * list that is not a ShadowListUtilsInboxList.
   */
  private val showsCheck: Boolean get() = editing || (row as? ShadowListUtilsInboxRow)?.editing == true

  override val showsPressedBackground: Boolean get() = highlighted || (showsCheck && isSelected)

  override fun fill(row: ShadowListUtilsRow) {
    val inbox = row as ShadowListUtilsInboxRow
    val colors = inbox.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = colors.fill
    unreadDotShape.setColor(colors.accent)
    flagShape.setColor(colors.orange)
    flag.visibility = if (inbox.item.flagged) VISIBLE else GONE
    checkIcon.color = colors.onAccent
    avatar.theme = inbox.theme
    avatar.show(inbox.item.sender, inbox.item.avatarUrl)
    updateState()
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val inbox = row as ShadowListUtilsInboxRow
    val frames = layout as ShadowListUtilsInboxRowLayout
    val colors = inbox.theme.colors
    unreadDot.place(frames.unreadDotFrame)
    check.place(frames.checkFrame)
    val iconInset = (frames.checkFrame.width() - CHECK_ICON_SIZE_DP.px) / 2
    checkIcon.place(Rect(frames.checkFrame).apply { inset(iconInset, iconInset) })
    avatar.place(frames.avatarFrame)
    sender.show(frames.sender, colors.label, frames.senderFrame)
    if (flag.visibility == VISIBLE) flag.place(frames.flagFrame)
    time.show(frames.time, colors.secondaryLabel, frames.timeFrame)
    subject.show(frames.subject, colors.label, frames.subjectFrame)
    preview.show(frames.preview, colors.secondaryLabel, frames.previewFrame)
  }

  override fun setSelected(selected: Boolean, animated: Boolean) {
    super.setSelected(selected, animated)
    updateState()
    updateBackground()
  }

  override fun setEditing(editing: Boolean, animated: Boolean) {
    super.setEditing(editing, animated)
    updateState()
    updateBackground()
  }

  /*
   * The gutter shows the unread dot, or the selection circle while editing. The label reads the
   * row's states before its text.
   */
  private fun updateState() {
    val inbox = row as? ShadowListUtilsInboxRow ?: return
    val item = inbox.item
    val colors = inbox.theme.colors
    val editing = showsCheck
    val on = editing && isSelected
    unreadDot.visibility = if (editing || item.read) GONE else VISIBLE
    check.visibility = if (editing) VISIBLE else GONE
    checkIcon.visibility = if (on) VISIBLE else GONE
    checkShape.setColor(if (on) colors.accent else 0)
    checkShape.setStroke(CHECK_BORDER_DP.px, if (on) colors.accent else colors.tertiaryLabel)
    val states = listOf(
      inbox.labels.unreadState.takeIf { !item.read },
      inbox.labels.flaggedState.takeIf { item.flagged },
      inbox.labels.selected.takeIf { on })
    contentDescription = (states + listOf(item.sender, item.subject, item.preview)).filterNotNull().joinToString(", ")
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    info.isSelected = showsCheck && isSelected
  }

  private companion object {
    const val FLAG_RADIUS_DP = 2f
    const val CHECK_BORDER_DP = 1.5f
    const val CHECK_ICON_SIZE_DP = 14f
  }
}
