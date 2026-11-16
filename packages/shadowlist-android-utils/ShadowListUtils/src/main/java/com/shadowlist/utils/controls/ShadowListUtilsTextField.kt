package com.shadowlist.utils

import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.text.InputType
import android.view.Gravity
import android.widget.FrameLayout

/*
 * Controlled field or code/text block; only native text input owns marked text and selection.
 */
class ShadowListUtilsTextField(context: Context, value: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  var value = value
    set(value) { field = value; editing.value = value }
  var onChange: ((String) -> Unit)? = null
  var placeholder: String? = null
    set(value) { field = value; input.hint = value; input.contentDescription = accessibilityLabel ?: value }
  var keyboardType = ShadowListUtilsKeyboardType.DEFAULT
    set(value) { field = value; configure() }
  var multiline = false
    set(value) { if (field != value) { field = value; configure(); requestLayout() } }
  var accessibilityLabel: String? = null
    set(value) { field = value; input.contentDescription = value ?: placeholder }
  internal val input = ShadowListUtilsTextInputEditText(context)
  private val editing = ShadowListUtilsTextInput(input, value)
  private val defaultPaddingTop = input.paddingTop
  private val defaultPaddingBottom = input.paddingBottom

  init {
    background = GradientDrawable().apply { setColor(theme.colors.fill); cornerRadius = 9f.px.toFloat() }
    addView(input, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    input.background = null
    input.setTextColor(theme.colors.label)
    input.setHintTextColor(theme.colors.secondaryLabel)
    editing.value = value
    editing.onChangeText = { onChange?.invoke(it) }
    configure()
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec((if (multiline) 104f else 40f).px, MeasureSpec.EXACTLY))
  }
  override fun requestFocus(direction: Int, previouslyFocusedRect: android.graphics.Rect?): Boolean = input.requestFocus(direction, previouslyFocusedRect)
  private fun configure() = editing.configure {
    editing.configureSubmit(multiline)
    input.imeOptions = android.view.inputmethod.EditorInfo.IME_ACTION_DONE
    var type = keyboardType.inputType
    if ((type and InputType.TYPE_MASK_CLASS) == InputType.TYPE_CLASS_TEXT) {
      type = type or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
      if (multiline) type = type or InputType.TYPE_TEXT_FLAG_MULTI_LINE
    }
    if (input.inputType != type) input.inputType = type
    input.setSingleLine(!multiline)
    input.gravity = Gravity.TOP or Gravity.START
    input.textSize = if (multiline) 14f else theme.typography.body.fontSize
    input.typeface = if (multiline) Typeface.MONOSPACE else Typeface.DEFAULT
    input.letterSpacing = if (multiline) (-0.6f).spf / input.textSize else theme.typography.body.letterSpacing.spf / input.textSize
    // React Native floors a TextInput's padding to pixels.
    val paddingHorizontal = kotlin.math.floor(12f.pxf).toInt()
    input.setPadding(paddingHorizontal, if (multiline) 10f.px else defaultPaddingTop, paddingHorizontal, if (multiline) 10f.px else defaultPaddingBottom)
    if (!multiline) input.gravity = Gravity.CENTER_VERTICAL or Gravity.START
  }
}
