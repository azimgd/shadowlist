package com.shadowlist.utils

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout

class ShadowListUtilsIconButton(context: Context, icon: View, accessibilityLabel: String, val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  private var hitArea: ShadowListUtilsTouchDelegateGroup? = null
  var icon = icon
    set(value) { removeView(field); field = value; installIcon() }
  var onPress: (() -> Unit)? = null
    set(value) { field = value; isEnabled = value != null }
  var accessibilityLabel = accessibilityLabel
    set(value) { field = value; contentDescription = value }
  init {
    contentDescription = accessibilityLabel; isFocusable = true; isClickable = true; isEnabled = false
    val shape = GradientDrawable().apply { setColor(theme.colors.accentSoft); cornerRadius = 20f.pxf }
    val mask = GradientDrawable().apply { setColor(Color.WHITE); cornerRadius = 20f.pxf }
    background = ShadowListUtilsPressFeedback.background(theme, shape, mask)
    setOnClickListener { if (isEnabled) onPress?.invoke() }
    installIcon()
  }
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) { super.onMeasure(MeasureSpec.makeMeasureSpec(40f.px, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(40f.px, MeasureSpec.EXACTLY)) }
  override fun setPressed(pressed: Boolean) { super.setPressed(pressed); alpha = ShadowListUtilsPressFeedback.alpha(pressed, ripple = theme.pressFeedback == ShadowListUtilsThemePressFeedback.RIPPLE) }
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    super.onLayout(changed, left, top, right, bottom)
    updateHitArea()
  }
  override fun onAttachedToWindow() { super.onAttachedToWindow(); updateHitArea() }
  override fun onDetachedFromWindow() { hitArea?.remove(this); hitArea = null; super.onDetachedFromWindow() }
  private fun updateHitArea() {
    if (!isAttachedToWindow || width <= 0 || height <= 0) return
    val parentView = parent as? View ?: return
    val group = ShadowListUtilsTouchDelegateGroup.forParent(parentView)
    if (hitArea !== group) { hitArea?.remove(this); hitArea = group }
    group.update(this, 4f.px)
  }
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) { super.onInitializeAccessibilityNodeInfo(info); info.className = android.widget.Button::class.java.name }
  private fun installIcon() {
    (icon.parent as? ViewGroup)?.removeView(icon)
    val original = icon.layoutParams
    addView(icon, LayoutParams(original?.width ?: LayoutParams.WRAP_CONTENT, original?.height ?: LayoutParams.WRAP_CONTENT, Gravity.CENTER))
  }
}
