package com.shadowlist.utils

import android.content.Context
import android.os.Build
import android.widget.TextView
import java.util.Locale

class ShadowListUtilsGroupedCaption(context: Context, title: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : TextView(context) {
  var title = title
    set(value) { field = value; applyTitle() }
  init {
    textSize = theme.typography.footnote.fontSize; setTextColor(theme.colors.secondaryLabel)
    val inset = (theme.grouped.inset + theme.grouped.rowInset).px
    setPadding(inset, 0, inset, (theme.spacing.sm - 2f).coerceAtLeast(0f).px)
    if (Build.VERSION.SDK_INT >= 28) isAccessibilityHeading = true
    applyTitle()
  }
  private fun applyTitle() { text = if (theme.grouped.captionCase == ShadowListUtilsCaptionCase.UPPER) title.uppercase(Locale.ROOT) else title }
}
