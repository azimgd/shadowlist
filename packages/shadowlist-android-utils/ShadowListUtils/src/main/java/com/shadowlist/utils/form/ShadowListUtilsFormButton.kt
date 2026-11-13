package com.shadowlist.utils

import android.content.Context
import android.view.Gravity
import android.widget.Button

class ShadowListUtilsFormButton(context: Context, label: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : Button(context) {
  var label = label
    set(value) { field = value; applyStyle() }
  var onPress: (() -> Unit)? = null
    set(value) { field = value; isEnabled = value != null }
  var destructive = false
    set(value) { field = value; applyStyle() }
  init {
    stateListAnimator = null; elevation = 0f; translationZ = 0f
    isAllCaps = false; gravity = Gravity.CENTER
    minHeight = (theme.tapTarget + 6f).px; minimumHeight = (theme.tapTarget + 6f).px
    setPadding(theme.grouped.rowInset.px, 8f.px, theme.grouped.rowInset.px, 8f.px)
    background = android.graphics.drawable.GradientDrawable().apply { setColor(theme.colors.groupedCell); cornerRadius = theme.grouped.radius.pxf }; isEnabled = false
    setOnClickListener { onPress?.invoke() }; applyStyle()
  }
  override fun setPressed(pressed: Boolean) { super.setPressed(pressed); alpha = ShadowListUtilsPressFeedback.alpha(pressed) }
  private fun applyStyle() {
    text = label; contentDescription = label
    theme.typography.body.applyTo(this, if (destructive) theme.colors.red else theme.colors.accent)
    maxLines = if (resources.configuration.fontScale >= 1.3f) 2 else 1
  }
}
