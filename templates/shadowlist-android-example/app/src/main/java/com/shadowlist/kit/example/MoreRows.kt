package com.shadowlist.kit.example

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.view.View
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.roundToInt

// region Reorder

class ReorderRow(val contact: Contact) : Row {
  override val key get() = contact.id
  override val viewType get() = "reorder"
  override fun makeView(context: Context): RowView = ReorderRowView(context)

  override fun layout(width: Int): RowLayout {
    val textWidth = width - 68.dp - 16.dp - 20.dp - 12.dp
    return ContactLayout(width, 67.dp, TextStyle.body.layout(contact.author.name, textWidth, 1),
      TextStyle.subhead.layout(contact.subtitle, textWidth, 1))
  }
}

/*
 * Three short lines, the drag handle of a reorderable row.
 */
class GripView(context: Context) : View(context) {
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
    color = Theme.tertiaryLabel
    strokeWidth = 2.dpf
    strokeCap = Paint.Cap.ROUND
  }

  override fun onDraw(canvas: Canvas) {
    val left = 2.dpf
    val right = width - 2.dpf
    for (line in 0 until 3) {
      val y = height / 2f + (line - 1) * 5.dpf
      canvas.drawLine(left, y, right, y, paint)
    }
  }
}

class ReorderRowView(context: Context) : RowView(context, "reorder") {
  private val avatar = AvatarView(context, 40.dp)
  private val name = ShadowListKitTextView(context)
  private val subtitle = ShadowListKitTextView(context)
  private val grip = GripView(context)
  private val separator = SeparatorView(context)

  init {
    for (view in listOf(avatar, name, subtitle, grip, separator)) addView(view)
  }

  override fun fill(row: Row) {
    avatar.show((row as ReorderRow).contact.author)
  }

  override fun apply(row: Row, layout: RowLayout) {
    val contact = layout as ContactLayout
    val width = contact.width
    avatar.place(16.dp, 13.5f.dp, 40.dp, 40.dp)
    name.show(contact.name, Theme.label, 68.dp, 12.dp)
    subtitle.show(contact.subtitle, Theme.secondaryLabel, 68.dp, 35.dp)
    grip.place(width - 16.dp - 20.dp, 23.5f.dp, 20.dp, 20.dp)
    separator.place(68.dp, 67.dp - Theme.hairline, width - 68.dp, Theme.hairline)
  }
}

/*
 * A numbered tile of a reorderable grid. Heights differ like a masonry gallery.
 */
class TileRow(val number: Int) : Row {
  override val key get() = "tile-$number"
  override val viewType get() = "tile"
  override fun makeView(context: Context): RowView = TileView(context)

  val color: Int get() = Theme.avatarPalette[number % Theme.avatarPalette.size]

  override fun layout(width: Int): RowLayout {
    val aspect = Photo.designHeights[number % Photo.designHeights.size] / 200f
    val height = (width * aspect).roundToInt()
    val title = TextStyle.largeTitle.layout("$number", width, 1)
    return TileLayout(width, height, title)
  }
}

class TileLayout(width: Int, height: Int, val title: ShadowListKitTextLayout) : RowLayout(width, height)

class TileView(context: Context) : RowView(context, "tile") {
  private val card = View(context)
  private val title = ShadowListKitTextView(context)

  init {
    // The lifted tile shows its rounded card only, not a square behind it.
    setBackgroundColor(0)
    addView(card)
    addView(title)
    card.clipToOutline = true
    card.outlineProvider = object : android.view.ViewOutlineProvider() {
      override fun getOutline(view: View, outline: android.graphics.Outline) {
        outline.setRoundRect(0, 0, view.width, view.height, 12.dpf)
      }
    }
  }

  override fun fill(row: Row) {
    card.setBackgroundColor((row as TileRow).color)
  }

  override fun apply(row: Row, layout: RowLayout) {
    val tile = layout as TileLayout
    card.place(4.dp, 4.dp, tile.width - 8.dp, tile.height - 8.dp)
    title.show(tile.title, Theme.onAccent, (tile.width - tile.title.width) / 2, (tile.height - tile.title.height) / 2)
  }
}

// endregion

// region Snap

/*
 * A destination card a quarter of the screen tall, like the UIKit and React Native cards.
 */
class SnapRow(val index: Int, private val height: Int) : Row {
  override val key get() = "snap-$index"
  override val viewType get() = "snap"
  override fun makeView(context: Context): RowView = SnapCardView(context)

  val title: String get() = FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.size]
  val color: Int get() = Theme.avatarPalette[index % Theme.avatarPalette.size]
  val image: String get() = Fixtures.imageUrl(index, 800)

  override fun layout(width: Int): RowLayout {
    val card = Rect(8.dp, 8.dp, width - 8.dp, height - 8.dp)
    val title = TextStyle(20f, true, 25f).layout(title, card.width() - 32.dp, 2)
    return SnapLayout(width, height, card, title)
  }
}

class SnapLayout(width: Int, height: Int, val card: Rect, val title: ShadowListKitTextLayout) : RowLayout(width, height)

class SnapCardView(context: Context) : RowView(context, "snap") {
  private val image = RemoteImageView(context).apply { cornerRadius = 16.dpf }
  private val caption = View(context).apply { setBackgroundColor(0x59000000) }
  private val title = ShadowListKitTextView(context)

  init {
    addView(image)
    addView(caption)
    addView(title)
  }

  override fun fill(row: Row) {
    image.setBackgroundColor((row as SnapRow).color)
  }

  override fun apply(row: Row, layout: RowLayout) {
    val snap = layout as SnapLayout
    image.place(snap.card)
    image.setImage((row as SnapRow).image, snap.card.width(), snap.card.height())
    val captionHeight = snap.title.height + 32.dp
    caption.place(snap.card.left, snap.card.bottom - captionHeight, snap.card.width(), captionHeight)
    title.show(snap.title, Theme.onAccent, snap.card.left + 16.dp, snap.card.bottom - captionHeight + 16.dp)
  }
}

// endregion

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
    val text = TextStyle.subheadSemibold.layout(title, 400.dp, 1)
    return (text.width + 32.dp).coerceIn(96.dp, 260.dp)
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
    card.setBackgroundColor(if (item.header) Theme.elevated2 else item.color)
    title.textLayout = TextStyle.subheadSemibold.layout(item.title, 400.dp, 1)
    title.textColor = if (item.header) Theme.secondaryLabel else Theme.onAccent
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
