package com.shadowlist.utils

import android.content.Context
import android.view.Gravity
import android.widget.TextView

/*
 * Read-only trailing value, retaining the application value independently of its empty glyph.
 */
class ShadowListUtilsFormValue(context: Context, text: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : android.widget.FrameLayout(context) {
  var text = text
    set(value) { field = value; label.text = value.ifEmpty { "—" }; updateAccessibility(); requestLayout() }
  var accessibilityLabel: String? = null
    set(value) { field = value; updateAccessibility() }
  private val label = TextView(context).apply {
    this.text = this@ShadowListUtilsFormValue.text.ifEmpty { "—" }; setTextColor(theme.colors.secondaryLabel)
    textSize = theme.typography.body.fontSize; fontFeatureSettings = "tnum"
    gravity = Gravity.RIGHT or Gravity.CENTER_VERTICAL; maxLines = 1; ellipsize = android.text.TextUtils.TruncateAt.END
  }
  init {
    addView(label, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
    label.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    updateAccessibility()
  }
  private fun updateAccessibility() { theme.typography.body.applyTo(label, theme.colors.secondaryLabel); contentDescription = accessibilityLabel ?: text.ifEmpty { "—" } }
}
