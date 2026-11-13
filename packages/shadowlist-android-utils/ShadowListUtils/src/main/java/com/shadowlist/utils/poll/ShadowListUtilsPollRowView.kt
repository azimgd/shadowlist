package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.widget.ImageView
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextView

/*
 * Draws the percentage and the app's selected state. A press does not change either on its own.
 */
class ShadowListUtilsPollRowView(context: Context) : ShadowListUtilsRowView(context, ShadowListUtilsPollRow.REUSE_IDENTIFIER) {
  private val selection = View(context)
  private val icon = ImageView(context).apply { scaleType = ImageView.ScaleType.FIT_CENTER }
  private val initial = ShadowListKitTextView(context)
  private val label = ShadowListKitTextView(context)
  private val percent = ShadowListKitTextView(context)
  private val bar = View(context)
  private val barFill = View(context)
  private val selectionShape = GradientDrawable().apply { shape = GradientDrawable.OVAL }
  private val barShape = GradientDrawable()
  private val barFillShape = GradientDrawable()

  init {
    selection.background = selectionShape
    bar.background = barShape
    barFill.background = barFillShape
    for (view in listOf(selection, icon, initial, label, percent, bar, barFill)) addView(view)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val poll = row as ShadowListUtilsPollRow
    val colors = poll.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = if (poll.interactive) colors.fill else null
    selectionShape.setColor(if (poll.isLeading) colors.accentSoft else colors.elevated)
    selectionShape.setStroke(if (poll.isSelected) 2f.px else 0, colors.accent)
    icon.setImageDrawable(poll.item.icon)
    icon.setColorFilter(if (poll.isLeading) colors.accent else colors.label)
    icon.visibility = if (poll.item.icon == null) GONE else VISIBLE
    initial.visibility = if (poll.item.icon == null) VISIBLE else GONE
    barShape.setColor(colors.fill)
    barFillShape.setColor(if (poll.isLeading) colors.accent else colors.secondaryLabel)
    contentDescription = poll.accessibilityLabel
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val poll = row as ShadowListUtilsPollRow
    val frames = layout as ShadowListUtilsPollRowLayout
    selection.place(frames.selectionFrame)
    icon.place(Rect(frames.selectionFrame).apply { inset(11f.px, 11f.px) })
    initial.show(frames.initial, if (poll.isLeading) poll.theme.colors.accent else poll.theme.colors.label, frames.initialFrame)
    label.show(frames.label, poll.theme.colors.label, frames.labelFrame)
    percent.show(frames.percent, if (poll.isLeading) poll.theme.colors.accent else poll.theme.colors.secondaryLabel, frames.percentFrame)
    bar.place(frames.barFrame)
    barFill.place(frames.barFillFrame)
    barShape.cornerRadius = frames.barFrame.height() / 2f
    barFillShape.cornerRadius = frames.barFillFrame.height() / 2f
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    val poll = row as? ShadowListUtilsPollRow ?: return
    info.className = "android.widget.RadioButton"
    info.isCheckable = true
    info.isChecked = poll.isSelected
    info.isClickable = poll.interactive
  }
}
