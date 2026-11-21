package com.shadowlist.kit

import android.view.View

/* Visual visibility alone does not include an ancestor's accessibility-hidden subtree. */
internal fun View.isAccessibilityVisible(): Boolean {
  if (!isAttachedToWindow || !isShown) return false
  var current: View? = this
  while (current != null) {
    if (current.importantForAccessibility == View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS) return false
    current = current.parent as? View
  }
  return true
}
