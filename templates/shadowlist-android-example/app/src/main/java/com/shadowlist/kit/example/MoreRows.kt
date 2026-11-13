package com.shadowlist.kit.example

import android.content.Context
import android.view.View
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitTextView
import com.shadowlist.utils.ShadowListUtilsFontWeight
import com.shadowlist.utils.ShadowListUtilsTheme

// region Horizontal

/*
 * An item of the horizontal list: a section's letter, pinned while its section scrolls by,
 * or a card as wide as its title.
 */
class StripItem(val key: String, val title: String, val header: Boolean, val color: Int) {
  /*
   * The width of the item, from its title. Safe on any thread.
   */
  fun width(): Int {
    if (header) return 44.dp
    val text = titleStyle.layout(title, 400.dp, 1)
    return (text.width + 32.dp).coerceIn(96.dp, 260.dp)
  }

  companion object {
    val titleStyle = ShadowListUtilsTheme.LIGHT.typography.subhead.with(ShadowListUtilsFontWeight.SEMIBOLD)
  }
}

/*
 * A cell that knows its own width for an exact height, for a horizontal list that measures
 * its cells.
 */
class StripCell(context: Context) : ShadowListKitListCell(context, "strip") {
  private val card = View(context)
  private val title = ShadowListKitTextView(context)
  private var item: StripItem? = null

  init {
    addView(card)
    addView(title)
    card.clipToOutline = true
    card.outlineProvider = object : android.view.ViewOutlineProvider() {
      override fun getOutline(view: View, outline: android.graphics.Outline) {
        outline.setRoundRect(0, 0, view.width, view.height, 12.dpf)
      }
    }
  }

  val key: String? get() = item?.key

  fun configure(item: StripItem) {
    this.item = item
    val colors = context.listTheme.colors
    card.setBackgroundColor(if (item.header) colors.elevated2 else item.color)
    title.textLayout = StripItem.titleStyle.layout(item.title, 400.dp, 1)
    title.textColor = if (item.header) colors.secondaryLabel else colors.onAccent
    requestLayout()
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val height = MeasureSpec.getSize(heightMeasureSpec)
    val width = if (MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY) {
      MeasureSpec.getSize(widthMeasureSpec)
    } else {
      item?.width() ?: 0
    }
    setMeasuredDimension(width, height)
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    val width = right - left
    val height = bottom - top
    card.place(4.dp, 8.dp, width - 8.dp, height - 16.dp)
    val text = title.textLayout ?: return
    title.place((width - text.width) / 2, (height - text.height) / 2, text.width, text.height)
  }
}

// endregion
