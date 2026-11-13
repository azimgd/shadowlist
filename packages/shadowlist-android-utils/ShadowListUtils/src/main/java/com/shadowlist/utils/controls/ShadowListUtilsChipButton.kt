package com.shadowlist.utils

import android.content.Context
import android.widget.Button

internal class ShadowListUtilsChipButton(context: Context) : Button(context) {
  var onHighlight: ((Boolean) -> Unit)? = null
  init {
    stateListAnimator = null; elevation = 0f; translationZ = 0f
    background = null; isAllCaps = false
    minWidth = 0; minimumWidth = 0; minHeight = 0; minimumHeight = 0; setPadding(0, 0, 0, 0)
  }
  override fun setPressed(pressed: Boolean) { super.setPressed(pressed); onHighlight?.invoke(pressed) }
}
