package com.shadowlist.kit.example

import android.content.Context
import android.view.View
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import com.shadowlist.utils.ShadowListUtilsFontWeight
import com.shadowlist.utils.ShadowListUtilsListFooter
import com.shadowlist.utils.ShadowListUtilsTheme

// region Horizontal

/*
 * An item of the horizontal list: a section's letter, pinned while its section scrolls by,
 * or a card as wide as its title.
 */
class StripItem(val key: String, val title: String, val header: Boolean, val color: Int) {
  /*
   * The typeset title, one line that truncates at the widest card.
   */
  val titleLayout: ShadowListKitTextLayout
    get() = titleStyle.layout(title, (MAX_WIDTH_DP - 2 * PADDING_HORIZONTAL_DP).dp, 1)

  /*
   * The width of the item, from its title. Safe on any thread.
   */
  fun width(): Int {
    if (header) return HEADER_WIDTH_DP.dp
    return (titleLayout.width + (2 * PADDING_HORIZONTAL_DP).dp).coerceIn(MIN_WIDTH_DP.dp, MAX_WIDTH_DP.dp)
  }

  companion object {
    val titleStyle = ShadowListUtilsTheme.LIGHT.typography.subhead.with(ShadowListUtilsFontWeight.SEMIBOLD)
    const val HEIGHT_DP = 140
    private const val HEADER_WIDTH_DP = 44
    private const val MIN_WIDTH_DP = 96
    private const val MAX_WIDTH_DP = 260
    private const val PADDING_HORIZONTAL_DP = 16
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
        outline.setRoundRect(0, 0, view.width, view.height, context.listTheme.radius.md.dpf)
      }
    }
  }

  val key: String? get() = item?.key

  fun configure(item: StripItem) {
    this.item = item
    val colors = context.listTheme.colors
    card.setBackgroundColor(if (item.header) colors.elevated2 else item.color)
    title.textLayout = item.titleLayout
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
    card.place(CARD_INSET_HORIZONTAL_DP.dp, CARD_INSET_VERTICAL_DP.dp, width - (2 * CARD_INSET_HORIZONTAL_DP).dp, height - (2 * CARD_INSET_VERTICAL_DP).dp)
    val text = title.textLayout ?: return
    title.place((width - text.width) / 2, (height - text.height) / 2, text.width, text.height)
  }

  private companion object {
    const val CARD_INSET_HORIZONTAL_DP = 4
    const val CARD_INSET_VERTICAL_DP = 8
  }
}

// endregion

// region List footer

/*
 * The library's ListFooter as a cell: a section's footer, or an item that is a caption. The
 * list sizes it through onMeasure.
 */
class ListFooterCell(context: Context) : ShadowListKitListCell(context, "footer") {
  private var footer: ShadowListUtilsListFooter? = null

  fun configure(text: String) {
    if (footer?.text == text) return
    removeAllViews()
    footer = ShadowListUtilsListFooter(context, text, context.listTheme).also {
      addView(it, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    }
  }
}

// endregion
