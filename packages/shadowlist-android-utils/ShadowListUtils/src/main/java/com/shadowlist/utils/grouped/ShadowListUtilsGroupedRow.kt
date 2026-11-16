package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.StateListDrawable
import android.view.Gravity
import android.view.Menu
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration

/*
 * A grouped row whose primary press and enclosing swipe/menu surface have separate owners.
 */
class ShadowListUtilsGroupedRow(context: Context, title: String, theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context)) : FrameLayout(context) {
  var title = title
    set(value) { field = value; refresh() }
  var subtitle: String? = null
    set(value) { field = value; refresh() }
  var subtitleLines = 1
    set(value) { field = value; refresh() }
  var leading: View? = null
    set(value) { field = value; refresh() }
  var trailing: View? = null
    set(value) { field = value; refresh() }
  var chevron: Boolean? = null
    set(value) { field = value; refresh() }
  var onPress: (() -> Unit)? = null
  var onLongPress: (() -> Unit)? = null
    set(value) { field = value; refreshLongPress() }
  var longPressLabel: String? = null
    set(value) { field = value; refreshLongPress() }
  var disabled = false
    set(value) { field = value; refresh() }
  var separated = false
    set(value) { field = value; refresh() }
  var accessibilityRole = ShadowListUtilsGroupedRowAccessibilityRole.BUTTON
    set(value) { field = value; sendAccessibilityEvent(android.view.accessibility.AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) }
  var testID: String? = null
    set(value) { field = value; sendAccessibilityEvent(android.view.accessibility.AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) }
  var theme = theme
    set(value) { field = value; actions.theme = value; refresh() }
  var leadingSwipeActions: ShadowListKitSwipeActionsConfiguration? = null
    set(value) { field = value; actions.leadingSwipeActions = value }
  var trailingSwipeActions: ShadowListKitSwipeActionsConfiguration? = null
    set(value) { field = value; actions.trailingSwipeActions = value }
  var contextMenu: ((Menu) -> Boolean)? = null
    set(value) { field = value; actions.contextMenu = value }

  private val primary = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
  private val body = FrameLayout(context)
  private val bodyRow = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
  private val texts = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
  private val titleView = TextView(context)
  private val subtitleView = TextView(context)
  private val chevronView = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.CHEVRON)
  private val separator = View(context)
  private val actions = ShadowListUtilsGroupedActions(context, listOf(primary), theme)
  private var longPressAction = View.NO_ID

  init {
    actions.useExternalAccessibilityOwner()
    addView(actions, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    body.addView(bodyRow, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    body.addView(separator, LayoutParams(LayoutParams.MATCH_PARENT, 1, Gravity.TOP))
    ViewCompat.setScreenReaderFocusable(this, true)
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
    isFocusable = true
    isClickable = true
    primary.importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
    primary.setOnClickListener { performClick() }
    primary.setOnLongClickListener {
      val callback = onLongPress
      if (disabled || callback == null) false else { callback(); true }
    }
    refresh()
    refreshLongPress()
  }

  fun closeSwipeActions(animated: Boolean = true) = actions.closeSwipeActions(animated)

  override fun performClick(): Boolean {
    if (disabled) return false
    super.performClick()
    onPress?.invoke()
    return true
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    info.className = if (accessibilityRole == ShadowListUtilsGroupedRowAccessibilityRole.BUTTON) "android.widget.Button" else "android.view.View"
    if (accessibilityRole == ShadowListUtilsGroupedRowAccessibilityRole.LINK) AccessibilityNodeInfoCompat.wrap(info).roleDescription = context.getString(R.string.shadowlist_grouped_link)
    info.viewIdResourceName = testID
    info.isEnabled = !disabled
  }

  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    refresh()
  }

  private fun refreshLongPress() {
    if (longPressAction != View.NO_ID) ViewCompat.removeAccessibilityAction(this, longPressAction)
    longPressAction = View.NO_ID
    val label = longPressLabel
    if (onLongPress != null && label != null) {
      longPressAction = ViewCompat.addAccessibilityAction(this, label) { _, _ ->
        val callback = onLongPress ?: return@addAccessibilityAction false
        callback()
        true
      }
    }
  }

  private fun refresh() {
    val large = ShadowListUtilsPlatform.isLargeText
    titleView.text = title
    titleView.maxLines = if (large) 3 else 1
    titleView.ellipsize = android.text.TextUtils.TruncateAt.END
    theme.typography.body.applyTo(titleView, theme.colors.label)
    subtitleView.text = subtitle
    subtitleView.maxLines = if (large) maxOf(subtitleLines, 3) else if (subtitleLines <= 0) Int.MAX_VALUE else subtitleLines
    subtitleView.ellipsize = android.text.TextUtils.TruncateAt.END
    theme.typography.footnote.applyTo(subtitleView, theme.colors.secondaryLabel)
    texts.replaceChildren(if (subtitle != null) listOf(titleView, subtitleView) else listOf(titleView))
    /*
     * React Native rounds the row's height once and floors a text origin inside the text
     * column to the pixel grid. The paddings and the gap between the texts follow the same rule.
     */
    val padding = theme.spacing.sm + PADDING_EXTRA_DP
    val gap = if (subtitle != null) TEXT_GAP_DP else 0f
    val paddingTop = padding.px
    val textGap = kotlin.math.floor(gap.pxf).toInt()
    val paddingBottom = (2 * padding + gap).px - paddingTop - textGap
    subtitleView.layoutParams = LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT).apply { topMargin = textGap }
    texts.layoutParams = LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
    val rest = mutableListOf<View>(texts)
    trailing?.let { rest.add(it) }
    if (chevron ?: theme.grouped.chevron) rest.add(chevronView)
    bodyRow.replaceChildren(rest)
    for (child in rest.drop(1)) {
      val old = child.layoutParams
      child.layoutParams = LinearLayout.LayoutParams(if (child === chevronView) 16f.px else old?.width ?: LayoutParams.WRAP_CONTENT,
        if (child === chevronView) 16f.px else old?.height ?: LayoutParams.WRAP_CONTENT).apply { marginStart = theme.spacing.sm.px }
    }
    chevronView.color = theme.colors.tertiaryLabel
    bodyRow.setPaddingRelative(0, paddingTop + if (separated) ShadowListUtilsTheme.HAIRLINE_PX else 0,
      theme.grouped.rowInset.px, paddingBottom)
    body.minimumHeight = theme.grouped.rowHeight.px
    bodyRow.minimumHeight = theme.grouped.rowHeight.px
    body.layoutParams = LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
    primary.replaceChildren(listOfNotNull(leading, body))
    leading?.let { child ->
      val old = child.layoutParams
      child.layoutParams = LinearLayout.LayoutParams(old?.width ?: LayoutParams.WRAP_CONTENT, old?.height ?: LayoutParams.WRAP_CONTENT).apply { marginEnd = theme.spacing.md.px }
    }
    primary.setPaddingRelative(theme.grouped.rowInset.px, 0, 0, 0)
    separator.setBackgroundColor(theme.colors.separator)
    separator.visibility = if (separated) VISIBLE else GONE
    val resting = ColorDrawable(theme.colors.groupedCell)
    primary.background = if (theme.pressFeedback == ShadowListUtilsThemePressFeedback.RIPPLE) {
      ShadowListUtilsPressFeedback.background(theme, resting, ColorDrawable(Color.WHITE))
    } else StateListDrawable().apply {
      addState(intArrayOf(android.R.attr.state_pressed), ColorDrawable(theme.colors.fill))
      addState(intArrayOf(), resting)
    }
    primary.isEnabled = !disabled
    primary.alpha = if (disabled) 0.4f else 1f
    contentDescription = if (subtitle == null) title else "$title, $subtitle"
    requestLayout()
  }

  private companion object {
    const val PADDING_EXTRA_DP = 2f
    const val TEXT_GAP_DP = 1f
  }
}
