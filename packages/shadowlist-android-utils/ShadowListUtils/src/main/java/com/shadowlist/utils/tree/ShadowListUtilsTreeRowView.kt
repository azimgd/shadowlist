package com.shadowlist.utils

import android.content.Context
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.ImageView
import com.shadowlist.kit.ShadowListKitTextView

class ShadowListUtilsTreeRowView(context: Context) : ShadowListUtilsRowView(context, ShadowListUtilsTreeRow.REUSE_IDENTIFIER) {
  private val name = ShadowListKitTextView(context)
  private val chevron = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.CHEVRON)
  private val icon = ImageView(context).apply { scaleType = ImageView.ScaleType.FIT_CENTER }
  private val count = ShadowListKitTextView(context)

  init { for (view in listOf(name, chevron, icon, count)) addView(view) }

  override fun fill(row: ShadowListUtilsRow) {
    val tree = row as ShadowListUtilsTreeRow
    val colors = tree.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = if (tree.interactive) colors.elevated else null
    chevron.color = colors.tertiaryLabel
    chevron.visibility = if (tree.hasChildren) VISIBLE else INVISIBLE
    chevron.rotation = if (tree.isExpanded) 90f else 0f
    val folder = (tree.item.kind ?: if (tree.item.children == null) ShadowListUtilsTreeNodeKind.FILE else ShadowListUtilsTreeNodeKind.FOLDER) == ShadowListUtilsTreeNodeKind.FOLDER
    icon.scaleType = if (tree.icon == null) ImageView.ScaleType.CENTER else ImageView.ScaleType.FIT_CENTER
    icon.setImageDrawable(tree.icon ?: if (folder) ShadowListUtilsFolderIcon(colors.accent, 20f) else ShadowListUtilsDocIcon(colors.secondaryLabel))
    if (tree.icon != null) icon.setColorFilter(colors.label) else icon.clearColorFilter()
    contentDescription = tree.item.name
    if (android.os.Build.VERSION.SDK_INT >= 30) stateDescription = if (tree.hasChildren) { if (tree.isExpanded) tree.labels.collapse else tree.labels.expand } else null
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val tree = row as ShadowListUtilsTreeRow
    val frames = layout as ShadowListUtilsTreeRowLayout
    name.show(frames.name, tree.theme.colors.label, frames.nameFrame)
    count.show(frames.count, tree.theme.colors.tertiaryLabel, frames.countFrame)
    count.visibility = if (frames.count == null) GONE else VISIBLE
    chevron.place(frames.chevronFrame)
    icon.place(frames.iconFrame)
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    val tree = row as? ShadowListUtilsTreeRow ?: return
    info.className = if (tree.interactive) "android.widget.Button" else "android.view.View"
    info.isClickable = tree.interactive
    if (tree.hasChildren) info.addAction(AccessibilityNodeInfo.AccessibilityAction(
      AccessibilityNodeInfo.ACTION_CLICK, if (tree.isExpanded) tree.labels.collapse else tree.labels.expand))
  }

}
