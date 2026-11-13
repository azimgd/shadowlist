package com.shadowlist.utils

import android.content.Context
import android.content.res.ColorStateList
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.ProgressBar

/*
 * The system activity indicator, centered with padding around it. For a list's footerView while
 * more items load. size is in dp. Set contentDescription for a label other than "Loading".
 */
class ShadowListUtilsSpinner(
  context: Context,
  val size: Float = DEFAULT_SIZE_DP,
  val color: Int? = null,
  theme: ShadowListUtilsTheme,
) : FrameLayout(context) {
  var theme: ShadowListUtilsTheme = theme
    set(value) {
      field = value
      applyTheme()
    }

  private val indicator = ProgressBar(context)

  init {
    val padding = theme.spacing.lg.px
    setPadding(padding, padding, padding, padding)
    indicator.isIndeterminate = true
    addView(indicator, LayoutParams(size.px, size.px, Gravity.CENTER))
    contentDescription = LOADING_LABEL
    applyTheme()
  }

  private fun applyTheme() {
    indicator.indeterminateTintList = ColorStateList.valueOf(color ?: theme.colors.secondaryLabel)
  }

  private companion object {
    const val DEFAULT_SIZE_DP = 20f
    const val LOADING_LABEL = "Loading"
  }
}
