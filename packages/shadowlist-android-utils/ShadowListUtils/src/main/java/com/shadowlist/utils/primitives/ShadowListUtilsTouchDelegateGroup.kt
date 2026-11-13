package com.shadowlist.utils

import android.graphics.Rect
import android.os.Build
import android.view.MotionEvent
import android.view.TouchDelegate
import android.view.View
import java.lang.ref.WeakReference
import java.util.WeakHashMap

/* Share a parent's native touch delegates without replacing another control's expanded target. */
internal open class ShadowListUtilsTouchDelegateGroup protected constructor(
  private val parent: View,
  protected val original: TouchDelegate?,
) : TouchDelegate(Rect(), parent) {
  protected data class Target(val view: View, val bounds: Rect, val touchView: ShadowListUtilsTouchDelegateView, val delegate: TouchDelegate)
  protected val targets = linkedMapOf<View, Target>()
  private var active: Target? = null

  fun update(view: View, padding: Int) {
    val bounds = Rect()
    view.getHitRect(bounds)
    bounds.inset(-padding, -padding)
    if (targets[view]?.bounds == bounds) return
    val touchView = ShadowListUtilsTouchDelegateView(view)
    touchView.syncSize()
    targets[view] = Target(view, bounds, touchView, TouchDelegate(Rect(bounds), touchView))
  }

  fun remove(view: View) {
    targets.remove(view)
    if (active?.view === view) active = null
    if (targets.isEmpty()) {
      if (parent.touchDelegate === this) parent.touchDelegate = original
      if (groups[parent]?.get() === this) groups.remove(parent)
    }
  }

  override fun onTouchEvent(event: MotionEvent): Boolean {
    if (event.actionMasked == MotionEvent.ACTION_DOWN) {
      active = null
      for (target in targets.values.toList()) {
        if (!target.view.isEnabled || !target.view.isShown) continue
        if (dispatch(target, event)) {
          if (targets[target.view] === target) active = target
          return true
        }
      }
    }
    val target = active
    if (target != null) {
      val handled = dispatch(target, event)
      if (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_CANCEL) active = null
      return handled
    }
    return original?.let { dispatch(it, event) } ?: false
  }

  private fun dispatch(target: Target, event: MotionEvent): Boolean {
    target.touchView.syncSize()
    return dispatch(target.delegate, event)
  }

  private fun dispatch(delegate: TouchDelegate, event: MotionEvent): Boolean {
    val copy = MotionEvent.obtain(event)
    try { return delegate.onTouchEvent(copy) } finally { copy.recycle() }
  }

  companion object {
    private val groups = WeakHashMap<View, WeakReference<ShadowListUtilsTouchDelegateGroup>>()
    fun forParent(parent: View): ShadowListUtilsTouchDelegateGroup {
      val existing = groups[parent]?.get()
      if (existing != null && parent.touchDelegate === existing) return existing
      val group = if (Build.VERSION.SDK_INT >= 29) {
        ShadowListUtilsTouchDelegateGroupApi29(parent, parent.touchDelegate)
      } else {
        ShadowListUtilsTouchDelegateGroup(parent, parent.touchDelegate)
      }
      groups[parent] = WeakReference(group)
      parent.touchDelegate = group
      return group
    }
  }
}
