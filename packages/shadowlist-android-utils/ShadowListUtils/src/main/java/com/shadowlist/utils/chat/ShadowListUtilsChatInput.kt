package com.shadowlist.utils

import android.content.Context
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.Gravity
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
      editing.value = value
      updateSend()
    }
  var disabled = false
    set(value) { field = value; updateSend() }
  var maxLength = 500
    set(value) { field = value; editing.maxLength = value }
  var labels = ShadowListUtilsChatLabels()
    set(value) { field = value; updateSend() }
  internal val input = ShadowListUtilsTextInputEditText(context)
  private val editing = ShadowListUtilsTextInput(input, defaultValue, initialMaxLength = maxLength)
  internal val send = Button(context)
  // React Native floors a TextInput's padding to pixels.
  private val paddingHorizontal = kotlin.math.floor(theme.spacing.md.pxf).toInt()

  init {
    orientation = HORIZONTAL
    isBaselineAligned = false
    gravity = Gravity.BOTTOM
    setPadding(theme.spacing.sm.px, theme.spacing.sm.px + ShadowListUtilsTheme.HAIRLINE_PX, theme.spacing.sm.px, theme.spacing.sm.px)
    setOnApplyWindowInsetsListener { _, insets ->
      @Suppress("DEPRECATION")
      setPadding(paddingLeft, paddingTop, paddingRight, insets.systemWindowInsetBottom.takeIf { it > 0 } ?: theme.spacing.sm.px)
      insets
    }
    background = android.graphics.drawable.LayerDrawable(arrayOf(
      android.graphics.drawable.ColorDrawable(theme.colors.background),
      android.graphics.drawable.ColorDrawable(theme.colors.separator),
    )).apply { setLayerHeight(1, ShadowListUtilsTheme.HAIRLINE_PX); setLayerGravity(1, Gravity.TOP) }
    input.setTextSize(TypedValue.COMPLEX_UNIT_PX, kotlin.math.ceil(theme.typography.body.fontSize.spf))
    input.typeface = Typeface.DEFAULT
    input.letterSpacing = theme.typography.body.letterSpacing.spf / input.textSize
    input.paint.isSubpixelText = true
    input.setTextColor(theme.colors.label)
    input.setHintTextColor(theme.colors.secondaryLabel)
    input.setPadding(
      paddingHorizontal + ShadowListUtilsTheme.HAIRLINE_PX, INPUT_PADDING_VERTICAL_DP.px + ShadowListUtilsTheme.HAIRLINE_PX,
      paddingHorizontal + ShadowListUtilsTheme.HAIRLINE_PX, INPUT_PADDING_VERTICAL_DP.px + ShadowListUtilsTheme.HAIRLINE_PX)
    input.includeFontPadding = false
    input.gravity = Gravity.CENTER_VERTICAL or Gravity.START
    androidx.core.widget.TextViewCompat.setLineHeight(input, theme.typography.body.scaledLineHeightPx())
    input.minHeight = maxOf(INPUT_MIN_HEIGHT_DP.px, theme.typography.body.scaledLineHeightPx() + (2 * INPUT_PADDING_VERTICAL_DP).px + 2 * ShadowListUtilsTheme.HAIRLINE_PX)
    input.maxHeight = 120f.px
    input.background = GradientDrawable().apply {
      setColor(theme.colors.background)
      cornerRadius = (theme.radius.lg + 2).pxf
      setStroke(1, theme.colors.separator)
    }
    editing.maxLength = maxLength
    addView(input, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f).apply { marginEnd = theme.spacing.sm.px })
    send.minWidth = 0
    send.minimumWidth = 0
    send.minHeight = 0
    send.minimumHeight = 0
    send.setPadding(0, 0, 0, 0)
    send.stateListAnimator = null
    send.elevation = 0f
    addView(send, LayoutParams(32f.px, 32f.px).apply { bottomMargin = theme.spacing.xxs.px })
    send.setOnClickListener { sendText() }
    editing.onChangeText = { onChangeText?.invoke(it) }
    editing.onContentSizeChange = { updateSend() }
    updateSend()
  }

  /*
   * The assistant supplies one rounded surface around the input and its toolbar.
   */
  internal fun useEmbeddedAppearance() {
    setBackgroundColor(android.graphics.Color.TRANSPARENT)
    setPadding(0, 0, 0, 0)
    setOnApplyWindowInsetsListener { _, insets -> insets }
    input.minHeight = INPUT_MIN_HEIGHT_DP.px
    input.background = null
    (input.layoutParams as LayoutParams).marginEnd = 0
    input.setPadding(paddingHorizontal, INPUT_PADDING_VERTICAL_DP.px, paddingHorizontal, INPUT_PADDING_VERTICAL_DP.px)
  }

  private fun updateSend() {
    input.isEnabled = !disabled
    input.hint = labels.placeholder
    input.contentDescription = labels.placeholder
    send.contentDescription = labels.send
    send.isEnabled = !disabled && ShadowListUtilsCore.trim(input.text.toString()).isNotEmpty()
    send.foreground = ShadowListUtilsArrowUpIcon(if (send.isEnabled) theme.colors.onAccent else theme.colors.secondaryLabel, 20f, 2.4f)
    send.background = GradientDrawable().apply {
      shape = GradientDrawable.OVAL
      setColor(if (send.isEnabled) theme.colors.accent else theme.colors.fill)
    }
  }

  internal fun sendText() {
    val text = ShadowListUtilsCore.trim(input.text.toString())
    if (disabled || text.isEmpty()) return
    val controlled = value != null
    onSend?.invoke(text)
    if (!controlled) {
      editing.clearDraft()
    }
  }

  internal companion object {
    const val INPUT_MIN_HEIGHT_DP = 36f
    private const val INPUT_PADDING_VERTICAL_DP = 7f
  }
}
