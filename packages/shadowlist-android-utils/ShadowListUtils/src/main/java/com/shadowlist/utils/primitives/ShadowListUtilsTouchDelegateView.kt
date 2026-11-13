package com.shadowlist.utils

import android.view.MotionEvent
import android.view.View

/*
 * TouchDelegate requires a View target and recenters expanded hits. Forward those hits to the
 * owner's native onTouchEvent so a nested icon button cannot steal the outer expanded action.
 * This adapter is never mounted or exposed to accessibility; the real view owns all state.
 */
internal class ShadowListUtilsTouchDelegateView(private val target: View) : View(target.context) {
  init { importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO; isFocusable = false }

  fun syncSize() {
    if (width != target.width || height != target.height) layout(0, 0, target.width, target.height)
  }
  override fun dispatchTouchEvent(event: MotionEvent): Boolean = target.onTouchEvent(event)
  override fun dispatchHoverEvent(event: MotionEvent): Boolean = target.onHoverEvent(event)
}
