package com.shadowlist.utils

import android.content.Context
import android.view.View
import android.widget.LinearLayout

class ShadowListUtilsGroupedSection(context: Context, title: String? = null, children: List<View> = emptyList(), val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : LinearLayout(context) {
  var title = title
    set(value) { field = value; updateTitle() }
  var children = children.toList()
    set(value) { field = value.toList(); card.children = field }
  private val caption = ShadowListUtilsGroupedCaption(context, title.orEmpty(), theme)
  private val card = ShadowListUtilsGroupedCard(context, children, theme)
  init { orientation = VERTICAL; setPadding(0, 0, 0, theme.spacing.xl.px); addView(caption); addView(card); updateTitle() }
  private fun updateTitle() { caption.title = title.orEmpty(); caption.visibility = if (title.isNullOrEmpty()) GONE else VISIBLE }
}
