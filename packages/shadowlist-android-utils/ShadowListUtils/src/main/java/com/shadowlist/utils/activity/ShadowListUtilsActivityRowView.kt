package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextView

/*
 * Shows an activity from its precomputed layout. Reuse resets its unread state and accessibility.
 */
class ShadowListUtilsActivityRowView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsActivityRow.REUSE_IDENTIFIER) {
  private val avatar = ShadowListUtilsAvatar(context, ShadowListUtilsActivityRow.AVATAR_SIZE_DP, ShadowListUtilsTheme.LIGHT)
  private val unreadDot = View(context)
  private val title = ShadowListKitTextView(context)
  private val text = ShadowListKitTextView(context)
  private val time = ShadowListKitTextView(context)
  private val unreadDotShape = GradientDrawable().apply { shape = GradientDrawable.OVAL }

  init {
    unreadDot.background = unreadDotShape
    for (view in listOf(avatar, unreadDot, title, text, time)) addView(view)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val activity = row as ShadowListUtilsActivityRow
    restingBackgroundColor = activity.theme.colors.background
    pressedBackgroundColor = if (activity.interactive) activity.theme.colors.fill else null
    unreadDotShape.setColor(activity.theme.colors.accent)
    unreadDot.visibility = if (activity.item.read) GONE else VISIBLE
    avatar.theme = activity.theme
    avatar.show(activity.item.actor.name, activity.item.actor.avatarUrl, activity.item.actor.avatarColor)
    contentDescription = activity.accessibilityLabel
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val activity = row as ShadowListUtilsActivityRow
    val frames = layout as ShadowListUtilsActivityRowLayout
    avatar.place(frames.avatarFrame)
    unreadDot.place(frames.unreadDotFrame)
    title.show(frames.title, activity.theme.colors.label, frames.titleFrame)
    text.visibility = if (frames.text == null) GONE else VISIBLE
    frames.text?.let { text.show(it, activity.theme.colors.secondaryLabel, frames.textFrame) }
    time.show(frames.time, activity.theme.colors.tertiaryLabel, frames.timeFrame)
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    val activity = row as? ShadowListUtilsActivityRow ?: return
    info.isClickable = activity.interactive
    info.className = if (activity.interactive) "android.widget.Button" else "android.view.View"
  }
}
