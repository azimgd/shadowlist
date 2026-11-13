package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.widget.FrameLayout
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.View
import com.shadowlist.kit.ShadowListKitTextView

class ShadowListUtilsBoardCardView(context: Context) : ShadowListUtilsRowView(context, ShadowListUtilsBoardCard.REUSE_IDENTIFIER) {
  private val card = FrameLayout(context)
  private val accent = View(context)
  private val title = ShadowListKitTextView(context)
  private val detail = ShadowListKitTextView(context)
  private val shape = GradientDrawable()
  private val accentShape = GradientDrawable()
  init {
    card.background = shape
    accent.background = accentShape
    addView(card)
    for (view in listOf(accent, title, detail)) card.addView(view)
  }
  override fun fill(row: ShadowListUtilsRow) {
    val item = row as ShadowListUtilsBoardCard
    restingBackgroundColor = Color.TRANSPARENT
    pressedBackgroundColor = null
    shape.setColor(item.theme.colors.background)
    shape.cornerRadius = item.theme.radius.md.pxf
    accentShape.setColor(item.color ?: Color.TRANSPARENT)
    accentShape.cornerRadius = item.theme.spacing.xs.pxf / 2
    contentDescription = item.accessibilityLabel ?: listOfNotNull(item.title, item.label, item.detail).joinToString(", ")
  }
  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val item = row as ShadowListUtilsBoardCard
    val frames = layout as ShadowListUtilsBoardCardLayout
    card.place(frames.card)
    fun inside(frame: Rect) = Rect(frame).apply { offset(-frames.card.left, -frames.card.top) }
    accent.place(inside(frames.accent))
    title.show(frames.title, item.theme.colors.label, inside(frames.titleFrame))
    detail.show(frames.detail, item.theme.colors.secondaryLabel, inside(frames.detailFrame))
  }
}
