package com.shadowlist.utils

import android.content.Context
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.ShapeDrawable
import android.graphics.drawable.shapes.PathShape
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView

/*
 * An image or file attachment with an optional application-owned remove action.
 */
class ShadowListUtilsAssistantAttachmentChip(
  context: Context,
  val attachment: ShadowListUtilsAssistantAttachment,
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
  onRemove: ((String) -> Unit)? = null,
) : ViewGroup(context) {
  var onRemove = onRemove
    set(value) { field = value; requestLayout() }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; remove.contentDescription = labels.removeAttachment(attachment.name) }
  private val image = ShadowListUtilsImageView(context)
  private val file = ImageView(context)
  private val name = ShadowListKitTextView(context)
  private val detail = ShadowListKitTextView(context)
  private val remove = Button(context)
  private var frames: Layout? = null

  init {
    background = GradientDrawable().apply { setColor(theme.colors.elevated); cornerRadius = theme.radius.md.pxf }
    image.cornerRadius = theme.radius.sm
    image.placeholderColor = if (attachment.kind == ShadowListUtilsAssistantAttachment.Kind.IMAGE) attachment.color ?: theme.colors.elevated2 else theme.colors.elevated2
    image.url = if (attachment.kind == ShadowListUtilsAssistantAttachment.Kind.IMAGE) attachment.uri else null
    file.visibility = if (attachment.kind == ShadowListUtilsAssistantAttachment.Kind.IMAGE) GONE else VISIBLE
    val path = Path().apply {
      addRoundRect(2.5f, 1f, 15.5f, 17f, 1f, 1f, Path.Direction.CW)
      moveTo(5f, 6f); lineTo(13f, 6f)
      moveTo(5f, 10f); lineTo(10f, 10f)
    }
    file.setImageDrawable(ShapeDrawable(PathShape(path, 18f, 18f)).apply {
      paint.color = attachment.color ?: theme.colors.secondaryLabel
      paint.style = Paint.Style.STROKE
      paint.strokeWidth = 1.6f
    })
    name.contentDescription = attachment.name
    detail.contentDescription = attachment.detail
    remove.text = "×"
    remove.setPadding(0, 0, 0, 0)
    remove.minWidth = 0
    remove.minimumWidth = 0
    remove.minHeight = 0
    remove.minimumHeight = 0
    remove.setTextColor(theme.colors.label)
    remove.background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(theme.colors.fill) }
    remove.contentDescription = labels.removeAttachment(attachment.name)
    remove.setOnClickListener { this.onRemove?.invoke(attachment.key) }
    listOf(image, file, name, detail, remove).forEach(::addView)
  }

  internal data class Layout(
    val widthPx: Int, val heightPx: Int, val imageFrame: Rect,
    val name: ShadowListKitTextLayout, val nameFrame: Rect,
    val detail: ShadowListKitTextLayout?, val detailFrame: Rect, val removeFrame: Rect,
  )

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val layout = layout(attachment, MeasureSpec.getSize(widthMeasureSpec), onRemove != null, theme)
    frames = layout
    setMeasuredDimension(resolveSize(layout.widthPx, widthMeasureSpec), resolveSize(layout.heightPx, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val layout = frames ?: return
    image.place(layout.imageFrame)
    file.place(Rect(layout.imageFrame).apply { inset(9f.px, 9f.px) })
    name.show(layout.name, theme.colors.label, layout.nameFrame)
    detail.visibility = if (layout.detail == null) GONE else VISIBLE
    detail.show(layout.detail, theme.colors.secondaryLabel, layout.detailFrame)
    remove.visibility = if (onRemove == null) GONE else VISIBLE
    remove.place(layout.removeFrame)
  }

  companion object {
    internal fun layout(attachment: ShadowListUtilsAssistantAttachment, widthPx: Int, removable: Boolean, theme: ShadowListUtilsTheme): Layout {
      val padding = (theme.spacing.xs + 2).px
      val width = minOf(220f.px, widthPx.coerceAtLeast(1))
      val removeWidth = if (removable) 20f.px + theme.spacing.xs.px else 0
      val textWidth = (width - padding - 36f.px - theme.spacing.sm.px - theme.spacing.md.px - removeWidth).coerceAtLeast(1)
      val name = theme.typography.footnote.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(attachment.name, textWidth, 1)
      val detail = attachment.detail?.takeIf { it.isNotEmpty() }?.let { theme.typography.caption.layout(it, textWidth) }
      val textHeight = name.height + (detail?.height ?: 0)
      val height = maxOf(36f.px, textHeight) + 2 * padding
      val actualWidth = minOf(width, padding + 36f.px + theme.spacing.sm.px + maxOf(name.width, detail?.width ?: 0) + theme.spacing.md.px + removeWidth)
      val direction = ShadowListUtilsDirection(actualWidth)
      val textX = padding + 36f.px + theme.spacing.sm.px
      val textY = (height - textHeight) / 2
      return Layout(actualWidth, height,
        direction.place(Rect(padding, (height - 36f.px) / 2, padding + 36f.px, (height + 36f.px) / 2)),
        name, direction.place(frameOf(textX, textY, name)), detail,
        direction.place(detail?.let { frameOf(textX, textY + name.height, it) } ?: Rect()),
        direction.place(Rect(actualWidth - theme.spacing.md.px - 20f.px, (height - 20f.px) / 2, actualWidth - theme.spacing.md.px, (height + 20f.px) / 2)))
    }
  }
}
