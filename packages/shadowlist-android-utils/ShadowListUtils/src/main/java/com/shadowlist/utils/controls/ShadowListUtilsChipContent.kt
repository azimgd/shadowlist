package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.text.TextUtils
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import androidx.core.widget.TextViewCompat
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.min
import kotlin.math.roundToInt

/* Shared native slots and text measurement for Chip and ChoiceChip; no application state. */
internal class ShadowListUtilsChipContent(context: Context, val theme: ShadowListUtilsTheme) : FrameLayout(context) {
  val button = ShadowListUtilsChipButton(context)
  val clearButton = ShadowListUtilsChipButton(context)
  var label = ""
  var onPress: (() -> Unit)? = null
  var onClear: (() -> Unit)? = null
  var variant = ShadowListUtilsChipVariant.TINTED
  var size = ShadowListUtilsChipSize.REGULAR
  var mono = false
  var maxFontSizeMultiplier: Float? = null
  var labelOverride: String? = null
  var clearLabelOverride: String? = null
  var fillOverride: Int? = null
  var textColorOverride: Int? = null
  var borderWidth = 0f
  var borderColor = android.graphics.Color.TRANSPARENT
  var leadingView: View? = null
    set(value) { field?.let { removeView(it) }; field = value; value?.let { addView(it) } }
  private var clearDelegate: ShadowListUtilsTouchDelegateGroup? = null

  init {
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    button.gravity = Gravity.CENTER; button.setSingleLine(true); button.ellipsize = TextUtils.TruncateAt.END
    button.setOnClickListener { onPress?.invoke() }
    button.onHighlight = { alpha = ShadowListUtilsPressFeedback.alpha(it && onPress != null) }
    clearButton.gravity = Gravity.CENTER; clearButton.text = "×"
    clearButton.setOnClickListener { onClear?.invoke() }
    addView(button); addView(clearButton)
    refresh()
  }

  fun refresh() {
    val compact = size == ShadowListUtilsChipSize.COMPACT
    val plain = variant == ShadowListUtilsChipVariant.PLAIN && !isSelected
    val style = if (compact) theme.typography.footnote else theme.typography.subhead
    val color = textColorOverride ?: if (isSelected) theme.colors.onAccent else if (plain) theme.colors.secondaryLabel else theme.colors.accent
    style.applyTo(button, color, ShadowListUtilsTextStyle.WEIGHT_MEDIUM)
    val metrics = resources.displayMetrics
    val maximum = maxFontSizeMultiplier?.takeIf { it >= 1f && it.isFinite() }
    if (maximum != null) {
      val font = min(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, style.fontSize, metrics), style.fontSize * metrics.density * maximum)
      val line = min(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, style.lineHeight, metrics), style.lineHeight * metrics.density * maximum)
      button.setTextSize(TypedValue.COMPLEX_UNIT_PX, font); TextViewCompat.setLineHeight(button, ceil(line).toInt())
    }
    if (mono) {
      button.typeface = if (Build.VERSION.SDK_INT >= 28) Typeface.create(Typeface.MONOSPACE, 500, false) else Typeface.MONOSPACE
      button.letterSpacing = -0.6f / style.fontSize
    }
    button.text = label; button.contentDescription = labelOverride ?: label; button.isSelected = isSelected
    clearButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
    clearButton.setTextColor(color); clearButton.alpha = 0.7f
    clearButton.contentDescription = clearLabelOverride ?: "Clear $label"
    clearButton.isSelected = false
    clearButton.visibility = if (onClear == null) GONE else VISIBLE
    val fill = fillOverride ?: if (isSelected) theme.colors.accent else if (plain) theme.colors.fill else theme.colors.accentSoft
    background = GradientDrawable().apply {
      setColor(fill); cornerRadius = (if (compact) 14f else 17f).pxf
      setStroke(borderWidth.px, borderColor)
    }
    alpha = ShadowListUtilsPressFeedback.alpha(button.isPressed && onPress != null)
    requestLayout(); invalidate()
  }

  private val inset: Float get() = if (size == ShadowListUtilsChipSize.COMPACT) 11f else 14f
  private val spacing: Float get() = if (size == ShadowListUtilsChipSize.COMPACT) 6f else 4f
  private val padding: Int get() = inset.px + borderWidth.px
  private val gap: Int get() = spacing.px
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val unspecified = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
    clearButton.measure(unspecified, unspecified)
    val trailing = if (onClear == null) 0 else clearButton.measuredWidth + gap
    val widthLimit = 240f.px
    button.setPadding(0, 0, 0, 0); button.measure(unspecified, unspecified)
    val labelWidth = button.measuredWidth
    /*
     * React Native rounds the chip's width once and floors the label's origin. The trailing
     * inset takes the rest of the width.
     */
    val insets = 2 * (inset + borderWidth).pxf + if (leadingView == null) 0f else (LEADING_SIZE_DP + spacing).pxf
    val desired = min(widthLimit, (labelWidth + insets).roundToInt() + trailing)
    val width = resolveSize(desired, widthMeasureSpec)
    val height = (if (size == ShadowListUtilsChipSize.COMPACT) 28f else 34f).px
    val direction = ShadowListUtilsDirection(width)
    val before = floor(insets - (inset + borderWidth).pxf).toInt()
    val after = (width - before - labelWidth).coerceAtLeast(padding + trailing)
    button.setPadding(if (direction.isRightToLeft) after else before, 0, if (direction.isRightToLeft) before else after, 0)
    button.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
    leadingView?.measure(MeasureSpec.makeMeasureSpec(LEADING_SIZE_DP.px, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(LEADING_SIZE_DP.px, MeasureSpec.EXACTLY))
    setMeasuredDimension(width, height)
  }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    button.layout(0, 0, width, height)
    val direction = ShadowListUtilsDirection(width)
    leadingView?.let { it.place(direction.place(Rect(padding, (height - LEADING_SIZE_DP.px) / 2, padding + LEADING_SIZE_DP.px, (height + LEADING_SIZE_DP.px) / 2))) }
    val clearWidth = clearButton.measuredWidth; val clearHeight = clearButton.measuredHeight
    clearButton.place(direction.place(Rect(width - padding - clearWidth, (height - clearHeight) / 2, width - padding, (height + clearHeight) / 2)))
    updateClearDelegate()
  }
  override fun onAttachedToWindow() { super.onAttachedToWindow(); updateClearDelegate() }
  override fun onDetachedFromWindow() { clearDelegate?.remove(clearButton); clearDelegate = null; super.onDetachedFromWindow() }
  private fun updateClearDelegate() {
    if (onClear == null || !isAttachedToWindow || width <= 0) {
      clearDelegate?.remove(clearButton); clearDelegate = null; return
    }
    // The full-bounds main button owns touch in the clear button's expanded region. Both use
    // this container's coordinates, so its standard delegate can forward to the sibling clear.
    val group = ShadowListUtilsTouchDelegateGroup.forParent(button)
    if (clearDelegate !== group) { clearDelegate?.remove(clearButton); clearDelegate = group }
    group.update(clearButton, 8f.px)
  }

  private companion object {
    const val LEADING_SIZE_DP = 14f
  }
}
