package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.ViewGroup
import android.widget.Button

/*
 * An activity title, optional subtitle and wrapping action buttons. Set it as headerView.
 */
class ShadowListUtilsActivityHeader(
  context: Context,
  val title: String,
  val subtitle: String? = null,
  val actions: List<ShadowListUtilsActivityHeaderAction> = emptyList(),
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
) : ViewGroup(context) {
  private val header = ShadowListUtilsListHeader(context, title, subtitle, theme)
  private val buttons = actions.map { action ->
    Button(context).apply {
      text = action.label
      isAllCaps = false
      contentDescription = action.accessibilityLabel ?: action.label
      setTextSize(TypedValue.COMPLEX_UNIT_SP, theme.typography.footnote.fontSize)
      typeface = ShadowListUtilsTextStyle.typeface(ShadowListUtilsTextStyle.WEIGHT_SEMIBOLD)
      setTextColor(theme.colors.accent)
      minWidth = 0
      minimumWidth = 0
      minHeight = 0
      minimumHeight = 0
      setPadding(14f.px, theme.spacing.sm.px, 14f.px, theme.spacing.sm.px)
      background = GradientDrawable().apply { setColor(theme.colors.accentSoft); cornerRadius = theme.radius.sm.pxf }
      action.icon?.let { icon ->
        val copy = icon.constantState?.newDrawable()?.mutate() ?: icon
        copy.setTint(theme.colors.accent)
        copy.setBounds(0, 0, 18f.px, 18f.px)
        setCompoundDrawablesRelative(copy, null, null, null)
        compoundDrawablePadding = theme.spacing.xs.px
      }
      setOnClickListener { action.onPress() }
    }
  }
  private val frames = ArrayList<Rect>()

  init {
    setBackgroundColor(theme.colors.background)
    addView(header)
    buttons.forEach(::addView)
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    header.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
    val inset = theme.spacing.lg.px
    val available = (width - 2 * inset).coerceAtLeast(1)
    var x = inset
    var y = header.measuredHeight - theme.spacing.md.px + 14f.px
    var lineHeight = 0
    frames.clear()
    val direction = ShadowListUtilsDirection(width)
    for (button in buttons) {
      button.measure(MeasureSpec.makeMeasureSpec(available, MeasureSpec.AT_MOST), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
      if (x > inset && x + button.measuredWidth > width - inset) {
        x = inset
        y += lineHeight + theme.spacing.sm.px
        lineHeight = 0
      }
      frames.add(direction.place(Rect(x, y, x + button.measuredWidth, y + button.measuredHeight)))
      lineHeight = maxOf(lineHeight, button.measuredHeight)
      x += button.measuredWidth + theme.spacing.sm.px
    }
    val height = if (buttons.isEmpty()) header.measuredHeight else y + lineHeight + theme.spacing.md.px
    setMeasuredDimension(width, resolveSize(height, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    header.layout(0, 0, right - left, header.measuredHeight)
    for ((index, button) in buttons.withIndex()) button.place(frames[index])
  }
}
