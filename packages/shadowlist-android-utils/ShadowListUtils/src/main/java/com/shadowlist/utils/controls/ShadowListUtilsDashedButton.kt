package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.widget.Button

class ShadowListUtilsDashedButton(context: Context, label: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : Button(context) {
  var label = label
    set(value) { field = value; applyStyle() }
  var onPress: (() -> Unit)? = null
    set(value) { field = value; updateInteraction() }
  var disabled = false
    set(value) { field = value; updateInteraction() }
  init {
    stateListAnimator = null; elevation = 0f; translationZ = 0f
    isAllCaps = false; gravity = Gravity.CENTER
    minWidth = 0; minimumWidth = 0; minHeight = theme.tapTarget.px; minimumHeight = theme.tapTarget.px
    setPadding(0, 8f.px, 0, 8f.px)
    background = GradientDrawable().apply {
      setColor(theme.colors.accentSoft); cornerRadius = 10f.pxf
      setStroke(1f.px, theme.colors.accent, 3f.pxf, 3f.pxf)
    }
    setOnClickListener { if (!disabled) onPress?.invoke() }
    applyStyle(); updateInteraction()
  }
  override fun onConfigurationChanged(newConfig: android.content.res.Configuration) { super.onConfigurationChanged(newConfig); applyStyle() }
  override fun setPressed(pressed: Boolean) { super.setPressed(pressed); updateFeedback() }
  private fun applyStyle() {
    text = label; contentDescription = label
    theme.typography.body.applyTo(this, theme.colors.accent, ShadowListUtilsTextStyle.WEIGHT_MEDIUM)
  }
  private fun updateInteraction() { isEnabled = !disabled && onPress != null; updateFeedback() }
  private fun updateFeedback() { alpha = ShadowListUtilsPressFeedback.alpha(isPressed, disabled) }
}
