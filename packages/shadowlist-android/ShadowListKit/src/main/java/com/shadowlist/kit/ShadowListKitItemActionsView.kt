package com.shadowlist.kit

import android.content.Context
import android.os.SystemClock
import android.view.GestureDetector
import android.view.Menu
import android.view.MotionEvent
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import androidx.core.view.ViewCompat

/*
 * Swipe actions and a native context menu around ordinary native content. The private cell
 * carries reveal translation, leaving application-owned child transforms and state intact.
 */
class ShadowListKitItemActionsView(context: Context) : FrameLayout(context) {
  val contentView = FrameLayout(context)
  var leadingSwipeActions: ShadowListKitSwipeActionsConfiguration? = null
    set(value) { field = value; configurationChanged() }
  var trailingSwipeActions: ShadowListKitSwipeActionsConfiguration? = null
    set(value) { field = value; configurationChanged() }
  var contextMenu: ((Menu) -> Boolean)? = null
    set(value) { field = value; menu.close() }
  var onContextMenuWillShow: (() -> Unit)? = null

  internal val cell = ShadowListKitListCell(context, null)
  private val swipe = ShadowListKitSwipeController(ShadowListKitItemActionsSwipeHost(this))
  private val menu = ShadowListKitContextMenu()
  private val swipeAccessibility = ShadowListKitSwipeAccessibility(this) { swipe to cell }
  private val contentAccessibility = java.util.IdentityHashMap<View, ShadowListKitSwipeAccessibility>()
  private var menuTookTouch = false
  private val detector = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
    override fun onDown(event: MotionEvent): Boolean = true
    override fun onLongPress(event: MotionEvent) {
      val configure = contextMenu ?: return
      if (swipe.isOpen || swipe.closingTouch) return
      val shown = menu.show(cell, configure, { hasCurrentContent() && !swipe.isOpen && contextMenu === configure }) {
        menuTookTouch = true
        onContextMenuWillShow?.invoke()
      }
      if (shown || menuTookTouch) {
        cancelChildTouches()
      }
    }
  })

  init {
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    cell.addView(contentView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    addView(cell, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
  }

  fun closeSwipeActions(animated: Boolean = true) = swipe.close(animated)

  private fun configurationChanged() {
    swipe.close(false)
    refreshAccessibility()
  }

  internal fun hasCurrentContent(): Boolean = isAttachedToWindow && cell.parent === this && cell.isShown

  private fun containsContent(target: View): Boolean {
    var current: android.view.ViewParent? = target.parent
    while (current != null) {
      if (current === contentView) return target.isAttachedToWindow
      current = current.parent
    }
    return false
  }

  private fun containsAccessibilityOwner(target: View): Boolean {
    if (!isAccessibilityVisible() || !target.isAccessibilityVisible()) return false
    if (containsContent(target)) return true
    var current: View? = this
    while (current != null) {
      if (current === target) return true
      current = current.parent as? View
    }
    return false
  }

  private fun refreshAccessibility() {
    // Keep the native container getter available to an explicit external row owner.
    swipeAccessibility.refresh()
    val exposed = ArrayList<View>()
    if (hasCurrentContent() && isAccessibilityVisible() && importantForAccessibility != IMPORTANT_FOR_ACCESSIBILITY_YES) {
      var owner = parent as? View
      while (owner != null) {
        if (owner.importantForAccessibility == IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS) break
        if (ViewCompat.isScreenReaderFocusable(owner) && owner.importantForAccessibility != IMPORTANT_FOR_ACCESSIBILITY_NO &&
          owner.importantForAccessibility != IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS) {
          exposed.add(owner)
          break
        }
        owner = owner.parent as? View
      }
      if (exposed.isEmpty()) contentView.addChildrenForAccessibility(exposed)
    }
    val stale = contentAccessibility.keys.filter { old -> exposed.none { it === old } }
    for (old in stale) contentAccessibility.remove(old)?.clear()
    for (target in exposed) {
      val owner = contentAccessibility[target] ?: ShadowListKitSwipeAccessibility(target) {
        if (containsAccessibilityOwner(target)) swipe to cell else null
      }.also { contentAccessibility[target] = it }
      owner.refresh()
    }
  }

  internal fun beginSwipe() {
    val now = SystemClock.uptimeMillis()
    val cancel = MotionEvent.obtain(now, now, MotionEvent.ACTION_CANCEL, 0f, 0f, 0)
    detector.onTouchEvent(cancel)
    cancel.recycle()
    cancelChildTouches()
  }

  private fun cancelChildTouches() {
    val now = SystemClock.uptimeMillis()
    val cancel = MotionEvent.obtain(now, now, MotionEvent.ACTION_CANCEL, 0f, 0f, 0)
    super.dispatchTouchEvent(cancel)
    cancel.recycle()
    parent?.requestDisallowInterceptTouchEvent(true)
  }

  override fun dispatchTouchEvent(event: MotionEvent): Boolean {
    detector.onTouchEvent(event)
    if (menuTookTouch) {
      if (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_CANCEL) menuTookTouch = false
      return true
    }
    if (swipe.handle(event)) return true
    return super.dispatchTouchEvent(event)
  }

  override fun onInterceptTouchEvent(event: MotionEvent): Boolean = swipe.closingTouch || super.onInterceptTouchEvent(event)
  override fun onTouchEvent(event: MotionEvent): Boolean = true

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    super.onLayout(changed, left, top, right, bottom)
    swipe.layout()
    refreshAccessibility()
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    refreshAccessibility()
  }

  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    cell.mountGeneration++
    refreshAccessibility()
  }

  override fun onDetachedFromWindow() {
    val now = SystemClock.uptimeMillis()
    val cancel = MotionEvent.obtain(now, now, MotionEvent.ACTION_CANCEL, 0f, 0f, 0)
    detector.onTouchEvent(cancel)
    cancel.recycle()
    menu.close()
    menuTookTouch = false
    swipe.reset()
    swipe.releaseTracker()
    swipeAccessibility.clear()
    contentAccessibility.values.forEach { it.clear() }
    contentAccessibility.clear()
    super.onDetachedFromWindow()
  }
}
