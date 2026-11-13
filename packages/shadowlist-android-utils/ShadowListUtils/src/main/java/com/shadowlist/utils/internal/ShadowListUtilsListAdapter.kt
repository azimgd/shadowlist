package com.shadowlist.utils

import android.view.Menu
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration

/*
 * The data source and delegate of a ShadowListUtilsList. It reads the list's rows, takes sizes
 * from the layout cache and hands the delegate calls a template needs back to the list.
 */
internal open class ShadowListUtilsListAdapter(protected val list: ShadowListUtilsList) :
  ShadowListKitListView.DataSource,
  ShadowListKitListView.Sizing,
  ShadowListKitListView.ContentVersions,
  ShadowListKitListView.Delegate {
  private val registered = HashSet<String>()

  /*
   * The header rows of a list with sections. A plain list has none.
   */
  open val headerRows: List<ShadowListUtilsRow> get() = emptyList()

  // region Cells

  /*
   * A cell showing the row. The list learns a reuse identifier the first time a row with it
   * arrives.
   */
  protected fun cell(row: ShadowListUtilsRow): ShadowListKitListCell {
    val identifier = row.reuseIdentifier
    if (registered.add(identifier)) list.registerCell(identifier) { row.makeView(it) }
    val cell = list.dequeueReusableCell<ShadowListUtilsRowView>(identifier)
    show(row, cell)
    return cell
  }

  /*
   * Configure a row view with the row and its layout from the cache, once the list knows its
   * cross size.
   */
  fun show(row: ShadowListUtilsRow, view: ShadowListUtilsRowView) {
    val cross = list.itemCrossSizePx
    view.configure(row, if (cross > 0) list.layoutCache.layout(row, cross) else null, cross)
  }

  /*
   * The row's size along the list from its layout: the height, or the width in a horizontal
   * list.
   */
  protected fun size(row: ShadowListUtilsRow, crossSize: Int): Int {
    list.itemCrossSizePx = crossSize
    val layout = list.layoutCache.layout(row, crossSize)
    return if (list.horizontal) layout.widthPx else layout.heightPx
  }

  // endregion

  // region Data

  override fun numberOfItems(listView: ShadowListKitListView): Int = list.rows.size

  override fun keyForItem(listView: ShadowListKitListView, index: Int): String = list.rows[index].key

  override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell = cell(list.rows[index])

  override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int =
    size(list.rows[index], crossSize)

  override fun contentVersionForItem(listView: ShadowListKitListView, index: Int): Long =
    list.contentVersion(list.rows[index])

  // endregion

  // region Events

  override fun didReachStart(listView: ShadowListKitListView) {
    list.onStartReached?.invoke()
  }

  override fun didReachEnd(listView: ShadowListKitListView) {
    list.onEndReached?.invoke()
  }

  override fun didBeginRefreshing(listView: ShadowListKitListView) {
    list.onRefresh?.invoke()
  }

  override fun didScroll(listView: ShadowListKitListView) = list.didScroll()

  // endregion

  // region Cells leaving

  override fun didEndDisplayingCell(listView: ShadowListKitListView, cell: ShadowListKitListCell, index: Int) {
    (cell as? ShadowListUtilsRowView)?.didEndDisplaying(list)
  }

  // endregion

  // region Selection

  override fun shouldSelectItem(listView: ShadowListKitListView, index: Int): Boolean = list.shouldSelectItem(index)

  override fun shouldHighlightItem(listView: ShadowListKitListView, index: Int): Boolean = list.shouldHighlightItem(index)

  override fun didSelectItem(listView: ShadowListKitListView, index: Int) = list.didSelectItem(index)

  override fun didDeselectItem(listView: ShadowListKitListView, index: Int) = list.didDeselectItem(index)

  // endregion

  // region Drag

  override fun moveItemToList(listView: ShadowListKitListView, sourceIndex: Int, destinationListId: String, destinationIndex: Int, destinationKey: String) =
    list.didMoveItemToList(sourceIndex, destinationListId, destinationIndex, destinationKey)

  override fun moveItem(listView: ShadowListKitListView, sourceIndex: Int, destinationIndex: Int) =
    list.didMoveItem(sourceIndex, destinationIndex)

  // endregion

  // region Swipe actions

  override fun leadingSwipeActionsForItem(listView: ShadowListKitListView, index: Int): ShadowListKitSwipeActionsConfiguration? =
    list.leadingSwipeActions(index)

  override fun trailingSwipeActionsForItem(listView: ShadowListKitListView, index: Int): ShadowListKitSwipeActionsConfiguration? =
    list.trailingSwipeActions(index)

  // endregion

  // region Menu

  override fun contextMenuForItem(listView: ShadowListKitListView, index: Int, menu: Menu): Boolean =
    list.contextMenu(index, menu)

  override fun didLongPressItem(listView: ShadowListKitListView, index: Int): Boolean = list.didLongPressItem(index)

  // endregion
}

/*
 * One section of a list with sections: its key, the row of its header and how many of the
 * list's rows are its items.
 */
internal class ShadowListUtilsListSection(val key: String, val header: ShadowListUtilsRow, val count: Int)

/*
 * The adapter of a list with sections. The list's rows are the items of every section in
 * order. The kit groups items in sections only for a data source that answers these calls.
 */
internal class ShadowListUtilsSectionListAdapter(list: ShadowListUtilsList) :
  ShadowListUtilsListAdapter(list), ShadowListKitListView.Sections {
  var sections: List<ShadowListUtilsListSection> = emptyList()
  var indexTitles: List<String>? = null

  override val headerRows: List<ShadowListUtilsRow> get() = sections.map { it.header }

  override fun numberOfSections(listView: ShadowListKitListView): Int = sections.size

  override fun numberOfItemsInSection(listView: ShadowListKitListView, section: Int): Int = sections[section].count

  override fun keyForSection(listView: ShadowListKitListView, section: Int): String = sections[section].key

  /*
   * A title, the header row's key here, gives the section its header. The cell below shows it.
   */
  override fun titleForHeaderInSection(listView: ShadowListKitListView, section: Int): String = sections[section].header.key

  override fun cellForHeaderInSection(listView: ShadowListKitListView, section: Int): ShadowListKitListCell =
    cell(sections[section].header)

  override fun sizeForHeaderInSection(listView: ShadowListKitListView, section: Int, crossSize: Int): Int =
    size(sections[section].header, crossSize)

  override fun sectionIndexTitles(listView: ShadowListKitListView): List<String>? = indexTitles
}
