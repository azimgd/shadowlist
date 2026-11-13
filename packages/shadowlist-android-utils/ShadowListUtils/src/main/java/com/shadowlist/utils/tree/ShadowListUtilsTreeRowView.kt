package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.ImageView
import com.shadowlist.kit.ShadowListKitTextView

class ShadowListUtilsTreeRowView(context: Context) : ShadowListUtilsRowView(context, ShadowListUtilsTreeRow.REUSE_IDENTIFIER) {
  private val name = ShadowListKitTextView(context)
  private val chevron = ShadowListUtilsIconView(context, ShadowListUtilsIconView.Kind.CHEVRON)
  private val icon = ImageView(context).apply { scaleType = ImageView.ScaleType.FIT_CENTER }
  private val defaultIcon = TreeGlyph(context)
  private val count = ShadowListKitTextView(context)

  init { for (view in listOf(name, chevron, icon, defaultIcon, count)) addView(view) }

  override fun fill(row: ShadowListUtilsRow) {
    val tree = row as ShadowListUtilsTreeRow
    val colors = tree.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = if (tree.interactive) colors.elevated else null
    chevron.color = colors.tertiaryLabel
    chevron.visibility = if (tree.hasChildren) VISIBLE else INVISIBLE
    chevron.rotation = if (tree.isExpanded) 90f else 0f
    icon.setImageDrawable(tree.icon)
    icon.setColorFilter(colors.label)
    icon.visibility = if (tree.icon == null) GONE else VISIBLE
    defaultIcon.visibility = if (tree.icon == null) VISIBLE else GONE
    defaultIcon.folder = (tree.item.kind ?: if (tree.item.children == null) ShadowListUtilsTreeNodeKind.FILE else ShadowListUtilsTreeNodeKind.FOLDER) == ShadowListUtilsTreeNodeKind.FOLDER
    defaultIcon.color = colors.label
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
    defaultIcon.place(frames.iconFrame)
  }

  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    val tree = row as? ShadowListUtilsTreeRow ?: return
    info.className = if (tree.interactive) "android.widget.Button" else "android.view.View"
    info.isClickable = tree.interactive
    if (tree.hasChildren) info.addAction(AccessibilityNodeInfo.AccessibilityAction(
      AccessibilityNodeInfo.ACTION_CLICK, if (tree.isExpanded) tree.labels.collapse else tree.labels.expand))
  }

  private class TreeGlyph(context: Context) : View(context) {
    var folder = false
      set(value) { field = value; invalidate() }
    var color = 0
      set(value) { field = value; invalidate() }
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()
    override fun onDraw(canvas: Canvas) {
      paint.color = color
      paint.style = Paint.Style.STROKE
      paint.strokeWidth = 1.5f.pxf
      path.reset()
      val w = width.toFloat()
      val h = height.toFloat()
      if (folder) {
        path.moveTo(w * 0.08f, h * 0.2f)
        path.lineTo(w * 0.42f, h * 0.2f)
        path.lineTo(w * 0.52f, h * 0.34f)
        path.lineTo(w * 0.92f, h * 0.34f)
        path.lineTo(w * 0.92f, h * 0.84f)
        path.lineTo(w * 0.08f, h * 0.84f)
      } else {
        path.moveTo(w * 0.2f, h * 0.08f)
        path.lineTo(w * 0.65f, h * 0.08f)
        path.lineTo(w * 0.84f, h * 0.28f)
        path.lineTo(w * 0.84f, h * 0.92f)
        path.lineTo(w * 0.2f, h * 0.92f)
      }
      path.close()
      canvas.drawPath(path, paint)
    }
  }
}
