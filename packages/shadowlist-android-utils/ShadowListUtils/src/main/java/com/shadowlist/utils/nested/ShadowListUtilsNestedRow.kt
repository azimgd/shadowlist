package com.shadowlist.utils

import android.content.Context
import android.graphics.Rect
import android.os.Build
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import kotlin.math.roundToInt

/*
 * A titled row that holds a horizontal list of cards. onPressCard runs for a tapped card with
 * the card and the row's item. Put these rows into a ShadowListUtilsList with setRows. The
 * horizontal list keeps its scroll position while its row is off screen.
 */
class ShadowListUtilsNestedRow(
  val item: ShadowListUtilsNestedItem,
  val theme: ShadowListUtilsTheme,
  val onPressCard: ((ShadowListUtilsNestedCardItem, ShadowListUtilsNestedItem) -> Unit)? = null,
) : ShadowListUtilsRow {
  override val key: String get() = item.key
  override val contentVersion: Int = item.hashCode()
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = ShadowListUtilsNestedRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val direction = ShadowListUtilsDirection(widthPx)
    val title = theme.typography.title3.layout(item.title, (widthPx - 2 * spacing.lg.px).coerceAtLeast(1), 1)
    val titleFrame = frameOf(spacing.lg.px, 0, title)
    // React Native rounds the list's edges and the row's height from sums of unrounded lengths.
    val listTop = title.height + spacing.md.pxf
    val listBottom = listTop + ShadowListUtilsNestedCard.heightPxf(theme)
    // The list is as tall as a card, and the row adds the space under it.
    val listFrame = Rect(0, listTop.roundToInt(), widthPx, listBottom.roundToInt())
    return ShadowListUtilsNestedRowLayout(
      widthPx, (listBottom + spacing.lg.pxf).roundToInt(), title, direction.place(titleFrame), listFrame)
  }

  internal companion object {
    const val REUSE_IDENTIFIER = "ShadowListUtilsNestedRow"
  }
}

class ShadowListUtilsNestedRowLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val title: ShadowListKitTextLayout,
  val titleFrame: Rect,
  val listFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class ShadowListUtilsNestedRowView(context: Context) :
  ShadowListUtilsRowView(context, ShadowListUtilsNestedRow.REUSE_IDENTIFIER) {
  private val title = ShadowListKitTextView(context)
  private val cards = ShadowListUtilsNestedCardList(context)

  /*
   * The key of the row the cards show now, and the key of the row whose saved place they have
   * yet to land on.
   */
  private var shownKey: String? = null
  private var restoringKey: String? = null

  init {
    addView(title)
    addView(cards)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) title.isAccessibilityHeading = true
  }

  override fun fill(row: ShadowListUtilsRow) {
    val nested = row as ShadowListUtilsNestedRow
    restingBackgroundColor = nested.theme.colors.background
    title.contentDescription = nested.item.title
    // A row that comes on screen lands where it was left. A row shown already keeps its place.
    if (shownKey != nested.key) {
      shownKey = nested.key
      restoringKey = nested.key
    }
    cards.show(nested)
  }

  override fun replaceRow(row: ShadowListUtilsRow) {
    super.replaceRow(row)
    (row as? ShadowListUtilsNestedRow)?.let(cards::replaceRow)
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val nested = row as ShadowListUtilsNestedRow
    val frames = layout as ShadowListUtilsNestedRowLayout
    title.show(frames.title, nested.theme.colors.label, frames.titleFrame)
    cards.place(frames.listFrame)
    restorePosition()
  }

  /*
   * The cards land where the reader left them, or at the start for a row seen the first time.
   * It waits for the cell to be in the outer list, which holds the saved places.
   */
  private fun restorePosition() {
    val key = restoringKey ?: return
    val outer = enclosingList ?: return
    restoringKey = null
    val state = (outer as? ShadowListUtilsList)?.nestedAnchors?.get(key)
    if (state != null) cards.restoreAnchorState(state) else cards.scrollToStart()
  }

  /*
   * Keep where the cards were, by the row's key, in the list that owns the row.
   */
  override fun didEndDisplaying(list: ShadowListUtilsList) {
    val key = row?.key ?: shownKey ?: return
    val state = cards.anchorState
    if (state != null) list.nestedAnchors[key] = state else list.nestedAnchors.remove(key)
    shownKey = null
  }
}

/*
 * The horizontal list of a nested row. A tap on a card goes to the row's onPressCard.
 */
internal class ShadowListUtilsNestedCardList(context: Context) : ShadowListUtilsList(context, ShadowListUtilsTheme.LIGHT) {
  private var nested: ShadowListUtilsNestedRow? = null

  init {
    horizontal = true
    allowsSelection = false
  }

  fun show(row: ShadowListUtilsNestedRow) {
    theme = row.theme
    replaceRow(row)
    setRows(row.item.cards.map { ShadowListUtilsNestedCard(it, row.theme) })
  }

  /*
   * Take a row with the same cards, like one built again with a new onPressCard.
   */
  fun replaceRow(row: ShadowListUtilsNestedRow) {
    nested = row
    allowsSelection = row.onPressCard != null
  }

  override fun didSelectItem(index: Int) {
    nested?.let { row -> row.item.cards.getOrNull(index)?.let { row.onPressCard?.invoke(it, row.item) } }
    deselectItem(index)
  }
}
