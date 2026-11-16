package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.text.InputType
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import kotlin.math.floor

class ShadowListUtilsSearchField(context: Context, value: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  var value = value
    set(value) { field = value; editing.value = value }
  var onChange: ((String) -> Unit)? = null
  var icon: View? = null
    set(value) { field = value; updateIcon() }
  var labels = ShadowListUtilsSearchFieldLabels()
    set(value) { field = value; input.hint = value.placeholder; input.contentDescription = value.placeholder }
  internal val input = ShadowListUtilsTextInputEditText(context)
  private val editing = ShadowListUtilsTextInput(input, value)
  private val defaultIcon = View(context).apply { background = ShadowListUtilsSearchIcon(theme.colors.secondaryLabel, 16f) }
  private var shownIcon: View? = null
  init {
    background = GradientDrawable().apply { setColor(theme.colors.elevated2); cornerRadius = 10f.px.toFloat() }
    addView(input, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT).apply { marginStart = 32f.px; marginEnd = 10f.px })
    // React Native centers the text in the field's unrounded height and floors its origin.
    input.background = null; input.setPadding(0, 0, 0, HEIGHT_DP.px - floor(HEIGHT_DP.pxf).toInt())
    input.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
    input.setSingleLine(true); input.gravity = Gravity.CENTER_VERTICAL or Gravity.START
    input.textSize = theme.typography.body.fontSize
    input.letterSpacing = theme.typography.body.letterSpacing.spf / input.textSize
    input.setTextColor(theme.colors.label); input.setHintTextColor(theme.colors.secondaryLabel)
    input.hint = labels.placeholder; input.contentDescription = labels.placeholder
    editing.configureSubmit(multiline = false)
    input.imeOptions = android.view.inputmethod.EditorInfo.IME_ACTION_DONE
    editing.value = value; editing.onChangeText = { onChange?.invoke(it) }
    defaultIcon.isClickable = false; defaultIcon.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
    updateIcon()
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val height = MeasureSpec.makeMeasureSpec(HEIGHT_DP.px, MeasureSpec.EXACTLY)
    super.onMeasure(widthMeasureSpec, height)
    val leading = 16f.px + (shownIcon?.measuredWidth ?: 0)
    val params = input.layoutParams as LayoutParams
    if (params.marginStart != leading) { params.marginStart = leading; super.onMeasure(widthMeasureSpec, height) }
  }
  override fun requestFocus(direction: Int, previouslyFocusedRect: android.graphics.Rect?): Boolean = input.requestFocus(direction, previouslyFocusedRect)
  private fun updateIcon() {
    val next = icon ?: defaultIcon
    if (shownIcon === next) return
    shownIcon?.let { removeView(it) }; shownIcon = next
    val original = next.layoutParams
    (next.parent as? android.view.ViewGroup)?.removeView(next)
    val width = if (icon == null) 16f.px else original?.width ?: LayoutParams.WRAP_CONTENT
    val height = if (icon == null) 16f.px else original?.height ?: LayoutParams.WRAP_CONTENT
    addView(next, LayoutParams(width, height, Gravity.START or Gravity.CENTER_VERTICAL).apply { marginStart = 10f.px })
  }

  private companion object {
    const val HEIGHT_DP = 36f
  }
}
