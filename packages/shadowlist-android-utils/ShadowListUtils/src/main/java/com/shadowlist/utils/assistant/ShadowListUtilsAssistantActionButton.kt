package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.Drawable
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.widget.Button

/*
 * An assistant action. Native images take the place of the React view's icon child.
 */
class ShadowListUtilsAssistantActionButton(
  context: Context,
  label: String,
  icon: Drawable? = null,
  showLabel: Boolean = false,
  disabled: Boolean = false,
  selected: Boolean = false,
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
  var onPress: (() -> Unit)? = null,
) : Button(context) {
  var label = label
    set(value) { field = value; apply() }
  var icon = icon
    set(value) { field = value; apply() }
  var showLabel = showLabel
    set(value) { field = value; apply() }

  init {
    isAllCaps = false
    minWidth = 32f.px
    minimumWidth = 32f.px
    minHeight = 32f.px
    minimumHeight = 32f.px
    setTextSize(TypedValue.COMPLEX_UNIT_SP, theme.typography.footnote.fontSize)
    typeface = ShadowListUtilsTextStyle.typeface(ShadowListUtilsTextStyle.WEIGHT_SEMIBOLD)
    isEnabled = !disabled
    isSelected = selected
    setOnClickListener { if (isEnabled) onPress?.invoke() }
    apply()
  }

  private fun apply() {
    contentDescription = label
    text = if (showLabel) label else ""
    setTextColor(theme.colors.label)
    setPadding(if (showLabel) 10f.px else 0, 0, if (showLabel) 10f.px else 0, 0)
    background = GradientDrawable().apply { setColor(if (showLabel) theme.colors.elevated else 0); cornerRadius = theme.radius.sm.pxf }
    val drawable = icon?.constantState?.newDrawable()?.mutate() ?: icon
    drawable?.setBounds(0, 0, 18f.px, 18f.px)
    setCompoundDrawablesRelative(drawable, null, null, null)
    compoundDrawablePadding = if (showLabel) theme.spacing.xs.px else 0
    alpha = if (isEnabled) 1f else 0.35f
  }

  override fun setEnabled(enabled: Boolean) {
    super.setEnabled(enabled)
    alpha = if (enabled) 1f else 0.35f
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(32f.px, MeasureSpec.EXACTLY))
  }
}
