package com.shadowlist.utils

import android.graphics.Rect
import android.graphics.Region
import android.view.MotionEvent
import android.view.TouchDelegate
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo.TouchDelegateInfo
import androidx.annotation.RequiresApi

/* API 29 accessibility metadata always names mounted controls, never the touch adapter. */
@RequiresApi(29)
internal class ShadowListUtilsTouchDelegateGroupApi29(parent: View, original: TouchDelegate?) :
  ShadowListUtilsTouchDelegateGroup(parent, original) {
  private val hoverDelegates = linkedMapOf<View, Pair<Rect, TouchDelegate>>()

  override fun getTouchDelegateInfo(): TouchDelegateInfo {
    // Android does not expose the View targets needed to merge an existing delegate's map.
    // Preserve its metadata; our controls keep their ordinary native accessibility nodes.
    original?.let { return it.touchDelegateInfo }
    val targetMap = linkedMapOf<Region, View>()
    for (target in targets.values) {
      if (target.view.isEnabled && target.view.isShown) targetMap[Region(target.bounds)] = target.view
    }
    return if (targetMap.isEmpty()) super.getTouchDelegateInfo() else TouchDelegateInfo(targetMap)
  }

  override fun onTouchExplorationHoverEvent(event: MotionEvent): Boolean {
    var handled = false
    hoverDelegates.keys.retainAll(targets.keys)
    for (target in targets.values.toList()) {
      if (!target.view.isEnabled || !target.view.isShown) continue
      val previous = hoverDelegates[target.view]
      val delegate = if (previous?.first == target.bounds) previous.second else {
        val bounds = Rect(target.bounds)
        TouchDelegate(bounds, target.touchView).also { hoverDelegates[target.view] = bounds to it }
      }
      target.touchView.syncSize()
      val copy = MotionEvent.obtain(event)
      try { handled = delegate.onTouchExplorationHoverEvent(copy) || handled } finally { copy.recycle() }
    }
    original?.let { delegate ->
      val copy = MotionEvent.obtain(event)
      try { handled = delegate.onTouchExplorationHoverEvent(copy) || handled } finally { copy.recycle() }
    }
    return handled
  }
}
