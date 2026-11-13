package com.shadowlist.utils

import android.content.Context
import android.graphics.Typeface
import android.text.InputType
import android.view.Gravity
import android.view.inputmethod.EditorInfo
import android.widget.FrameLayout

/* Controlled, borderless input; FormRow supplies its stacked/trailing alignment. */
class ShadowListUtilsFormInput(context: Context, value: String, accessibilityLabel: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  var value = value
    set(value) { field = value; editing.value = value }
  var onChange: ((String) -> Unit)? = null
  var placeholder: String? = null
    set(value) { field = value; input.hint = value }
  var numeric = false
    set(value) { field = value; applyConfiguration() }
  var keyboardType: ShadowListUtilsKeyboardType? = null
    set(value) { field = value; applyConfiguration() }
  var autoCapitalize: ShadowListUtilsAutoCapitalize? = null
    set(value) { field = value; applyConfiguration() }
  var autoCorrect: Boolean? = null
    set(value) { field = value; applyConfiguration() }
  var returnKeyType: ShadowListUtilsReturnKeyType? = null
    set(value) { field = value; applyConfiguration() }
  var onSubmitEditing: (() -> Unit)? = null
  var accessibilityLabel = accessibilityLabel
    set(value) { field = value; input.contentDescription = value }
  internal var stacked = false
    set(value) { field = value; input.gravity = (if (value) Gravity.LEFT else Gravity.RIGHT) or Gravity.CENTER_VERTICAL }
  internal val input = ShadowListUtilsTextInputEditText(context)
  private val editing = ShadowListUtilsTextInput(input, value)

  init {
    addView(input, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    input.background = null
    input.setPadding(0, 0, 0, 0)
    input.minHeight = 38f.px
    input.minimumHeight = 38f.px
    input.setHintTextColor(theme.colors.tertiaryLabel)
    input.setTextColor(theme.colors.label)
    input.contentDescription = accessibilityLabel
    editing.value = value
    editing.onChangeText = { onChange?.invoke(it) }
    editing.configureSubmit(multiline = false) { onSubmitEditing?.invoke() }
    applyConfiguration()
  }
  override fun requestFocus(direction: Int, previouslyFocusedRect: android.graphics.Rect?): Boolean = input.requestFocus(direction, previouslyFocusedRect)
  override fun onConfigurationChanged(newConfig: android.content.res.Configuration) { super.onConfigurationChanged(newConfig); applyConfiguration() }
  private fun applyConfiguration() = editing.configure {
    val literal = numeric || keyboardType == ShadowListUtilsKeyboardType.EMAIL_ADDRESS
    val keyboard = keyboardType ?: if (numeric) ShadowListUtilsKeyboardType.DECIMAL_PAD else ShadowListUtilsKeyboardType.DEFAULT
    var type = keyboard.inputType
    if ((type and InputType.TYPE_MASK_CLASS) == InputType.TYPE_CLASS_TEXT) {
      type = type or (autoCapitalize ?: if (literal) ShadowListUtilsAutoCapitalize.NONE else ShadowListUtilsAutoCapitalize.SENTENCES).inputFlag
      type = type or if (autoCorrect ?: !literal) InputType.TYPE_TEXT_FLAG_AUTO_CORRECT else InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
    }
    if (input.inputType != type) input.inputType = type
    input.imeOptions = returnKeyType?.imeAction ?: EditorInfo.IME_ACTION_DONE
    input.setSingleLine(true)
    input.gravity = (if (stacked) Gravity.LEFT else Gravity.RIGHT) or Gravity.CENTER_VERTICAL
    input.textSize = theme.typography.body.fontSize
    input.typeface = Typeface.DEFAULT
    input.fontFeatureSettings = if (numeric) "tnum" else null
    input.hint = placeholder
    input.letterSpacing = theme.typography.body.letterSpacing / theme.typography.body.fontSize
  }
}
