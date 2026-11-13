package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import com.shadowlist.kit.ShadowListKitTextView

/*
 * Draws the percentage and the app's selected state. A press does not change either on its own.
 */
class ShadowListUtilsPollRowView(context: Context) : ShadowListUtilsRowView(context, ShadowListUtilsPollRow.REUSE_IDENTIFIER) {
  private val selection = View(context)
  private val check = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.CHECK)
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
    for (view in listOf(selection, check, label, percent, bar, barFill)) addView(view)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val poll = row as ShadowListUtilsPollRow
    val colors = poll.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = if (poll.interactive) colors.fill else null
    selectionShape.setColor(if (poll.isSelected) colors.accent else 0)
    selectionShape.setStroke(1f.px, if (poll.isSelected) colors.accent else colors.tertiaryLabel)
    check.color = colors.onAccent
    check.visibility = if (poll.isSelected) VISIBLE else GONE
    barShape.setColor(colors.fill)
    barFillShape.setColor(if (poll.isLeading) colors.accent else colors.tertiaryLabel)
    contentDescription = poll.accessibilityLabel
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val poll = row as ShadowListUtilsPollRow
    val frames = layout as ShadowListUtilsPollRowLayout
    selection.place(frames.selectionFrame)
    check.place(Rect(frames.selectionFrame).apply { inset(5f.px, 5f.px) })
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
