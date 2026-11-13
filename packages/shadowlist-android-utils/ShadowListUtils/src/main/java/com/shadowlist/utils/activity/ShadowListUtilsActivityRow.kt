package com.shadowlist.utils

import android.content.Context
import android.text.SpannableString
import android.text.Spanned
import android.text.TextPaint
import android.text.style.ForegroundColorSpan
import android.text.style.MetricAffectingSpan
import android.graphics.Rect
import kotlin.math.max

/*
 * An activity row: the actor's avatar, their action, optional text and its age. Large text lets
 * the title wrap and gives the text four lines. Time is written once when the row is made.
 */
data class ShadowListUtilsActivityRow(
  val item: ShadowListUtilsActivityItem,
  val interactive: Boolean = false,
  val theme: ShadowListUtilsTheme,
  val labels: ShadowListUtilsActivityLabels = ShadowListUtilsActivityLabels(),
  val formatTime: ((Double) -> String)? = null,
) : ShadowListUtilsRow {
  val time: String = formatTime?.invoke(item.createdAtMs) ?: labels.time.format(item.createdAtMs, System.currentTimeMillis().toDouble())
  internal val accessibilityLabel = listOfNotNull(
    labels.unread.takeIf { !item.read }, "${item.actor.name} ${item.action}", item.text, time)
    .filter { it.isNotEmpty() }.joinToString(", ")

  override val key: String get() = item.key
  override val contentVersion: Int = 31 * (31 * item.hashCode() + interactive.hashCode()) + accessibilityLabel.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsActivityRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val typography = theme.typography
    val direction = ShadowListUtilsDirection(widthPx)
    val top = spacing.md.px
    val avatarSize = AVATAR_SIZE_DP.px
    val avatarFrame = Rect(spacing.lg.px, top, spacing.lg.px + avatarSize, top + avatarSize)
    val bodyX = avatarFrame.right + spacing.md.px
    val bodyWidth = (widthPx - bodyX - spacing.lg.px).coerceAtLeast(1)
    val time = typography.footnote.layout(this.time, (bodyWidth / 3).coerceAtLeast(1), 1)
    val timeFrame = frameOf(widthPx - spacing.lg.px - time.width, top, time)
    val titleWidth = (bodyWidth - time.width - spacing.sm.px).coerceAtLeast(1)
    val titleText = SpannableString("${item.actor.name} ${item.action}")
    titleText.setSpan(ForegroundColorSpan(theme.colors.secondaryLabel), 0, titleText.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    titleText.setSpan(ForegroundColorSpan(theme.colors.label), 0, item.actor.name.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    titleText.setSpan(object : MetricAffectingSpan() {
      override fun updateDrawState(paint: TextPaint) = updateMeasureState(paint)
      override fun updateMeasureState(paint: TextPaint) {
        paint.typeface = ShadowListUtilsTextStyle.typeface(ShadowListUtilsTextStyle.WEIGHT_SEMIBOLD)
      }
    }, 0, item.actor.name.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    val title = typography.subhead.layout(
      titleText, titleWidth, if (ShadowListUtilsPlatform.isLargeText) 0 else 1, scaled = true)
    val titleFrame = frameOf(bodyX, top, title)
    val text = item.text?.takeIf { it.isNotEmpty() }?.let {
      typography.footnote.layout(it, titleWidth, if (ShadowListUtilsPlatform.isLargeText) 4 else 2)
    }
    val textFrame = text?.let { frameOf(bodyX, titleFrame.bottom + spacing.xxs.px, it) } ?: Rect()
    val dotSize = spacing.sm.px
    val dotTop = top + (avatarSize - dotSize) / 2
    val unreadDotFrame = Rect(spacing.xs.px, dotTop, spacing.xs.px + dotSize, dotTop + dotSize)
    return ShadowListUtilsActivityRowLayout(
      widthPx, max(avatarFrame.bottom, max(titleFrame.bottom, textFrame.bottom)) + spacing.md.px,
      direction.place(avatarFrame), direction.place(unreadDotFrame), title, direction.place(titleFrame),
      text, direction.place(textFrame), time, direction.place(timeFrame))
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsActivityRow"
    const val AVATAR_SIZE_DP = 40f
  }
}
