package com.shadowlist.utils

import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.text.Editable
import android.text.InputFilter
import android.text.TextWatcher
import android.util.TypedValue
import android.view.Gravity
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.Button

/*
 * A multiline composer. null value uses the initial draft; a non-null value is application-owned.
 * Sending trims whitespace and only clears an uncontrolled draft.
 */
class ShadowListUtilsChatInput(
  context: Context,
  defaultValue: String = "",
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
) : LinearLayout(context) {
  var onSend: ((String) -> Unit)? = null
  var onChangeText: ((String) -> Unit)? = null
  var value: String? = null
    set(value) {
      field = value
      showText(value ?: draft)
    }
  var disabled = false
    set(value) { field = value; updateSend() }
  var maxLength = 500
    set(value) { field = value; input.filters = arrayOf(InputFilter.LengthFilter(value.coerceAtLeast(0))) }
  var labels = ShadowListUtilsChatLabels()
    set(value) { field = value; updateSend() }
  internal val input = EditText(context)
  internal val send = Button(context)
  private var draft = defaultValue
  private var updating = false

  init {
    orientation = HORIZONTAL
    gravity = Gravity.BOTTOM
    setPadding(theme.spacing.sm.px, theme.spacing.sm.px, theme.spacing.sm.px, theme.spacing.sm.px)
    setBackgroundColor(theme.colors.background)
    input.setTextSize(TypedValue.COMPLEX_UNIT_SP, theme.typography.body.fontSize)
    input.setTextColor(theme.colors.label)
    input.setHintTextColor(theme.colors.secondaryLabel)
    input.setPadding(theme.spacing.md.px, 7f.px, theme.spacing.md.px, 7f.px)
    input.minHeight = 36f.px
    input.maxHeight = 120f.px
    input.background = GradientDrawable().apply {
      setColor(theme.colors.background)
      cornerRadius = (theme.radius.lg + 2).pxf
      setStroke(1, theme.colors.separator)
    }
    input.filters = arrayOf(InputFilter.LengthFilter(maxLength))
    input.setText(defaultValue)
    addView(input, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f).apply { marginEnd = theme.spacing.sm.px })
    send.text = "↑"
    send.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20f)
    send.typeface = Typeface.DEFAULT_BOLD
    send.minWidth = 0
    send.minimumWidth = 0
    send.minHeight = 0
    send.minimumHeight = 0
    send.setPadding(0, 0, 0, 0)
    addView(send, LayoutParams(32f.px, 32f.px).apply { bottomMargin = theme.spacing.xxs.px })
    send.setOnClickListener { sendText() }
    input.addTextChangedListener(object : TextWatcher {
      override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {}
      override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) {}
      override fun afterTextChanged(text: Editable?) {
        if (updating) return
        val next = text.toString()
        if (value == null) draft = next
        onChangeText?.invoke(next)
        if (value != null) showText(value ?: draft)
        updateSend()
      }
    })
    updateSend()
  }

  private fun showText(text: String) {
    if (input.text.toString() != text) {
      updating = true
      input.setText(text)
      input.setSelection(input.text.length)
      updating = false
    }
    updateSend()
  }

  private fun updateSend() {
    input.isEnabled = !disabled
    input.hint = labels.placeholder
    input.contentDescription = labels.placeholder
    send.contentDescription = labels.send
    send.isEnabled = !disabled && input.text.toString().trim().isNotEmpty()
    send.setTextColor(if (send.isEnabled) theme.colors.onAccent else theme.colors.secondaryLabel)
    send.background = GradientDrawable().apply {
      shape = GradientDrawable.OVAL
      setColor(if (send.isEnabled) theme.colors.accent else theme.colors.fill)
    }
  }

  internal fun sendText() {
    val text = input.text.toString().trim()
    if (disabled || text.isEmpty()) return
    val controlled = value != null
    onSend?.invoke(text)
    if (!controlled) {
      draft = ""
      showText(value ?: draft)
    }
  }
}
