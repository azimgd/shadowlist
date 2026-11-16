package com.shadowlist.utils

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.widget.Button

class ShadowListUtilsPillButton(context: Context, label: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : Button(context) {
  private var labelColor = theme.colors.onAccent
  var label = label
    set(value) { field = value; applyStyle() }
  var onPress: (() -> Unit)? = null
    set(value) { field = value; updateInteraction() }
  var variant = ShadowListUtilsPillButtonVariant.PRIMARY
    set(value) { field = value; applyStyle() }
  var disabled = false
    set(value) { field = value; updateInteraction() }
  init {
    setLayerType(LAYER_TYPE_HARDWARE, null)
    stateListAnimator = null; elevation = 0f; translationZ = 0f
    isAllCaps = false; gravity = Gravity.CENTER
    minWidth = 72f.px; minimumWidth = 72f.px
    minHeight = 0; minimumHeight = 0; setPadding(14f.px, 0, 14f.px, 0)
    setOnClickListener { if (!disabled) onPress?.invoke() }
    applyStyle(); updateInteraction()
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) { super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(40f.px, MeasureSpec.EXACTLY)) }
  override fun onConfigurationChanged(newConfig: android.content.res.Configuration) { super.onConfigurationChanged(newConfig); applyStyle() }
  override fun setPressed(pressed: Boolean) { super.setPressed(pressed); updateFeedback() }
  private fun applyStyle() {
    text = label; contentDescription = label
    val emphasized = variant == ShadowListUtilsPillButtonVariant.PRIMARY || variant == ShadowListUtilsPillButtonVariant.TINTED
    val color = when (variant) {
      ShadowListUtilsPillButtonVariant.PRIMARY -> theme.colors.onAccent
      ShadowListUtilsPillButtonVariant.TINTED -> theme.colors.accent
      ShadowListUtilsPillButtonVariant.PLAIN -> theme.colors.secondaryLabel
      ShadowListUtilsPillButtonVariant.DESTRUCTIVE -> theme.colors.red
    }
    labelColor = color
    if (emphasized) theme.typography.body.applyTo(this, color, ShadowListUtilsTextStyle.WEIGHT_SEMIBOLD)
    else theme.typography.body.applyTo(this, color)
    val fill = when (variant) {
      ShadowListUtilsPillButtonVariant.PRIMARY -> theme.colors.accent
      ShadowListUtilsPillButtonVariant.TINTED -> theme.colors.fill
      else -> theme.colors.background
    }
    val shape = GradientDrawable().apply { setColor(fill); cornerRadius = 20f.pxf }
    val mask = GradientDrawable().apply { setColor(Color.WHITE); cornerRadius = 20f.pxf }
    background = ShadowListUtilsPressFeedback.background(theme, shape, mask)
    updateFeedback()
  }
  private fun updateInteraction() { isEnabled = !disabled && onPress != null; updateFeedback() }
  private fun updateFeedback() {
    alpha = ShadowListUtilsPressFeedback.alpha(isPressed, disabled, theme.pressFeedback == ShadowListUtilsThemePressFeedback.RIPPLE)
    // React Native applies feedback opacity to the background and the child label separately.
    setTextColor(labelColor.withOpacity(alpha))
  }
}
