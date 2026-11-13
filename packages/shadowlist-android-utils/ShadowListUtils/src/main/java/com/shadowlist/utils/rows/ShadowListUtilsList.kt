package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration
import android.view.Menu
import android.view.MotionEvent
import android.view.View
import com.shadowlist.kit.ShadowListKitAnchorState
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration
import kotlin.math.abs

/*
 * A ShadowListKitListView that shows rows. It is its own data source and delegate through one
 * adapter, sizes rows from their layouts and gives the app closures in place of a delegate.
 * Every property of the kit list stays reachable. Setting dataSource or delegate from outside
 * is a programmer error.
 *
 * A list made without a theme follows the device appearance. A list made with a theme, or given
 * one later, keeps it.
 */
open class ShadowListUtilsList private constructor(
  context: Context,
  theme: ShadowListUtilsTheme,
  private var followsSystemTheme: Boolean,
) : ShadowListKitListView(context) {

  constructor(context: Context) : this(context, ShadowListUtilsTheme.system(context), true)

  constructor(context: Context, theme: ShadowListUtilsTheme) : this(context, theme, false)

  private var storedTheme = theme

  /*
   * The theme the list draws its background with and the template lists build their rows with.
   * Setting it lays every row out again.
   */
  var theme: ShadowListUtilsTheme
    get() = storedTheme
    set(value) {
      followsSystemTheme = false
      if (storedTheme == value) return
      storedTheme = value
      reloadRows()
    }

  var rows: List<ShadowListUtilsRow> = emptyList()
    private set

  /*
   * Run when the scroll position comes near an edge.
   */
  var onEndReached: (() -> Unit)? = null
  var onStartReached: (() -> Unit)? = null

  /*
   * Setting a closure turns pull to refresh on, and null turns it off. Set the kit's refreshing
   * back to false when the refresh is done.
   */
  var onRefresh: (() -> Unit)? = null
    set(value) {
      field = value
      refreshEnabled = value != null
    }

  /*
   * Runs on every change of the offset.
   */
  var onScroll: (() -> Unit)? = null

  internal val layoutCache = ShadowListUtilsLayoutCache()

  /*
   * Where the horizontal lists of nested rows were when their rows left the screen, by row key.
   */
  internal val nestedAnchors = HashMap<String, ShadowListKitAnchorState>()

  /*
   * The cross size the kit last sized an item for: the list's width over its columns, or its
   * height when horizontal. Before the kit asks, it is worked out from the list's own size.
   */
  internal var itemCrossSizePx: Int = 0
    get() = when {
      field > 0 -> field
      horizontal -> height - paddingTop - paddingBottom
      else -> (width - paddingLeft - paddingRight) / numberOfColumns
    }

  private val checksAdapter = context.isDebuggable

  internal val adapter: ShadowListUtilsListAdapter = makeAdapter()

  /*
   * Mixed into every row's content version. A change of it configures every cell again, like
   * after a new theme.
   */
  private var contentGeneration = 0

  /*
   * The generation the kit last diffed the rows at.
   */
  private var appliedGeneration = 0

  /*
   * The last touch in the list's own coordinates, for rows with parts that take taps themselves.
   */
  private var touchX = 0f
  private var touchY = 0f

  private var prefetchOffset = 0

  /*
   * False while the constructors run, when the view already calls what the list overrides.
   */
  private var isConstructed = false

  // region Delegate

  /*
   * A debug build stops when something other than the list's adapter is set. A release build
   * keeps the adapter. Null passes, for a list that is torn down.
   */
  override var dataSource: DataSource?
    get() = super.dataSource
    set(value) {
      if (value != null && value !== adapter) {
        check(!checksAdapter) {
          "A ShadowListUtilsList is its own data source. Use a ShadowListKitListView with the library's rows instead."
        }
        return
      }
      super.dataSource = value
    }

  override var delegate: Delegate?
    get() = super.delegate
    set(value) {
      if (value != null && value !== adapter) {
        check(!checksAdapter) {
          "A ShadowListUtilsList is its own delegate. Use a ShadowListKitListView with the library's rows instead."
        }
        return
      }
      super.delegate = value
    }

  // endregion

  // The two properties above are declared before the list hands them its adapter.
  init {
    dataSource = adapter
    delegate = adapter
    setBackgroundColor(theme.colors.background)
    isConstructed = true
  }

  override fun onDetachedFromWindow() {
    super.onDetachedFromWindow()
    layoutCache.cancelPrefetch()
  }

  // region Data

  /*
   * Show these rows. The list diffs them by key against the ones it shows and configures the
   * cells whose content version changed. animated animates this change.
   */
  fun setRows(rows: List<ShadowListUtilsRow>, animated: Boolean = false) {
    val previous = this.rows
    this.rows = rows
    pruneLayouts()
    val animates = animatesChanges
    animatesChanges = animated
    val inserted = insertedIndices(previous, rows)
    if (inserted != null) insertItems(inserted) else applyChanges()
    animatesChanges = animates
    appliedGeneration = contentGeneration
    replaceMountedRows()
    setNeedsPrefetch()
  }

  /*
   * Stores rows the kit already holds in this order, like after a drop.
   */
  internal fun replaceRows(rows: List<ShadowListUtilsRow>) {
    this.rows = rows
  }

  /*
   * Where the new rows sit when the rows before are all still there, unchanged and in one run
   * at the start or the end: a pure append or prepend, which needs no diff. Null for any other
   * change, and for a list with sections, whose counts change with the rows.
   */
  private fun insertedIndices(previous: List<ShadowListUtilsRow>, rows: List<ShadowListUtilsRow>): IntArray? {
    val added = rows.size - previous.size
    if (previous.isEmpty() || added <= 0 || adapter.headerRows.isNotEmpty()) return null
    // A new generation configures every cell again, which only a diff does.
    if (appliedGeneration != contentGeneration) return null
    fun matches(offset: Int): Boolean {
      for (index in previous.indices) {
        val row = rows[index + offset]
        if (row === previous[index]) continue
        if (row.key != previous[index].key || row.contentVersion != previous[index].contentVersion) return false
      }
      return true
    }
    if (matches(0)) return IntArray(added) { previous.size + it }
    return if (matches(added)) IntArray(added) { it } else null
  }

  /*
   * The cache keeps the layouts of the rows and the section headers the list shows.
   */
  private fun pruneLayouts() {
    if (layoutCache.count == 0) return
    val keys = HashSet<String>(rows.size * 2)
    for (row in rows) keys.add(row.key)
    for (header in adapter.headerRows) keys.add(header.key)
    layoutCache.prune(keys)
  }

  /*
   * A mounted cell keeps its row object while the content version stays. Hand it the row built
   * now, which may carry a new closure.
   */
  private fun replaceMountedRows() {
    for (position in 0 until childCount) {
      val view = getChildAt(position) as? ShadowListUtilsRowView ?: continue
      val row = rows.getOrNull(view.index) ?: continue
      val shown = view.row ?: continue
      if (row !== shown && row.key == shown.key && row.contentVersion == shown.contentVersion) view.replaceRow(row)
    }
  }

  /*
   * The content version the kit compares for a row: the row's own under the list's generation.
   */
  internal fun contentVersion(row: ShadowListUtilsRow): Long =
    (contentGeneration.toLong() shl GENERATION_SHIFT) or (row.contentVersion.toLong() and 0xffffffffL)

  /*
   * A new width makes the layouts of the width before of no use.
   */
  override fun onSizeChanged(width: Int, height: Int, previousWidth: Int, previousHeight: Int) {
    super.onSizeChanged(width, height, previousWidth, previousHeight)
    val cross = if (horizontal) height else width
    val previousCross = if (horizontal) previousHeight else previousWidth
    if (cross == previousCross) return
    itemCrossSizePx = 0
    if (previousCross > 0) {
      layoutCache.invalidateAll()
      reloadData()
    }
    setNeedsPrefetch()
  }

  // endregion

  // region Template hooks

  /*
   * Called once, from the list's constructor.
   */
  internal open fun makeAdapter(): ShadowListUtilsListAdapter = ShadowListUtilsListAdapter(this)

  /*
   * Build the rows again from the template's items, after the theme or an option changed.
   * Returns null for a list whose rows the app sets.
   */
  internal open fun makeRows(): List<ShadowListUtilsRow>? = null

  /*
   * Show the template's rows for its items, and let the kit diff them against the rows before.
   */
  internal open fun rebuildRows(animated: Boolean = false) = setRows(makeRows() ?: rows, animated)

  /*
   * Everything the cells show may differ, like after a new theme, new labels or a new text
   * size: drop the layouts, build every row again and configure every cell again.
   */
  internal open fun reloadRows() {
    layoutCache.invalidateAll()
    ++contentGeneration
    applyTheme()
    rebuildRows()
  }

  /*
   * Set what the list itself takes from the theme.
   */
  internal open fun applyTheme() {
    setBackgroundColor(theme.colors.background)
  }

  internal open fun didSelectItem(index: Int) {}

  internal open fun didDeselectItem(index: Int) {}

  /*
   * An item without a menu was touched and held. Returns whether the list took the long press,
   * and the lift that follows is then not a tap.
   */
  internal open fun didLongPressItem(index: Int): Boolean = false

  /*
   * A held item was dropped at a new place. The kit reads the rows again right after.
   */
  internal open fun didMoveItem(sourceIndex: Int, destinationIndex: Int) {
    replaceRows(ShadowListUtilsArrayMove.moved(rows, sourceIndex, destinationIndex))
  }

  internal open fun leadingSwipeActions(index: Int): ShadowListKitSwipeActionsConfiguration? = null

  internal open fun trailingSwipeActions(index: Int): ShadowListKitSwipeActionsConfiguration? = null

  internal open fun contextMenu(index: Int, menu: Menu): Boolean = false

  /*
   * What a screen reader can do with an item besides pressing it: the template's closures that
   * are otherwise behind a gesture.
   */
  internal open fun didMoveItemToList(sourceIndex: Int, destinationListId: String, destinationIndex: Int, destinationKey: String) {}

  internal open fun accessibilityActions(index: Int): List<ShadowListUtilsAccessibilityAction> = emptyList()

  /*
   * Whether a tap selects or highlights an item. A part of a row with its own tap takes the
   * touch from the row.
   */
  internal open fun shouldSelectItem(index: Int): Boolean = rowViewClaimingTouch(index) == null
  internal open fun shouldHighlightItem(index: Int): Boolean = rowViewClaimingTouch(index) == null

  // endregion

  // region Text size

  /*
   * The reader chose a new text size, appearance or layout direction. A list that follows the
   * system takes the new appearance's theme, and every layout is made again.
   */
  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    if (followsSystemTheme) {
      val night = (newConfig.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
      storedTheme = if (night) ShadowListUtilsTheme.DARK else ShadowListUtilsTheme.LIGHT
    }
    reloadRows()
  }

  // endregion

  // region Prefetching

  /*
   * Lay out the rows around the first visible one on the cache's threads.
   */
  private fun setNeedsPrefetch() {
    val around = visibleRange?.first ?: if (inverted) rows.size - 1 else 0
    prefetchOffset = if (horizontal) scrollX else scrollY
    layoutCache.prefetch(rows, itemCrossSizePx, around)
  }

  internal fun didScroll() {
    onScroll?.invoke()
    val offset = if (horizontal) scrollX else scrollY
    val viewport = if (horizontal) width else height
    if (abs(offset - prefetchOffset) > viewport / 2) setNeedsPrefetch()
  }

  // endregion

  // region Items

  /*
   * The rows of the items. An item that equals the one shown under its key keeps its row,
   * which the list then neither builds nor lays out again.
   */
  internal fun <Item> rows(
    items: List<Item>,
    previousItems: List<Item>,
    previousRows: List<ShadowListUtilsRow>,
    key: (Item) -> String,
    make: (Item) -> ShadowListUtilsRow,
  ): List<ShadowListUtilsRow> {
    if (previousItems.size != previousRows.size || previousItems.isEmpty()) return items.map(make)
    val positions = HashMap<String, Int>(previousItems.size * 2)
    for ((position, item) in previousItems.withIndex()) positions[key(item)] = position
    return items.map { item ->
      val itemKey = key(item)
      val position = positions[itemKey]
      if (position != null && previousItems[position] == item && previousRows[position].key == itemKey) {
        previousRows[position]
      } else {
        make(item)
      }
    }
  }

  // endregion

  // region Direction

  /*
   * The view's layout direction is resolved, or changed. Rows laid out for the other direction
   * are laid out again.
   */
  override fun onRtlPropertiesChanged(layoutDirection: Int) {
    super.onRtlPropertiesChanged(layoutDirection)
    // The view's constructor resolves a direction before the list has its rows.
    if (isConstructed) resolveDirection()
  }

  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    resolveDirection()
  }

  private fun resolveDirection() {
    if (ShadowListUtilsPlatform.resolveDirection(layoutDirection == LAYOUT_DIRECTION_RTL) && rows.isNotEmpty()) reloadRows()
  }

  // endregion

  // region Touches

  override fun dispatchTouchEvent(event: MotionEvent): Boolean {
    if (event.actionMasked == MotionEvent.ACTION_DOWN || event.actionMasked == MotionEvent.ACTION_UP) {
      touchX = event.x
      touchY = event.y
    }
    return super.dispatchTouchEvent(event)
  }

  /*
   * The last touch in a cell's coordinates.
   */
  internal fun touchXIn(cell: View): Float = touchX + scrollX - cell.left - cell.translationX
  internal fun touchYIn(cell: View): Float = touchY + scrollY - cell.top - cell.translationY

  /*
   * The mounted row view of an item when a part of it with its own tap is under the last touch.
   */
  internal fun rowViewClaimingTouch(index: Int): ShadowListUtilsRowView? {
    val cell = cellForItem(index) as? ShadowListUtilsRowView ?: return null
    return cell.takeIf { it.claimsTouch(touchXIn(cell), touchYIn(cell)) }
  }

  // endregion

  private companion object {
    const val GENERATION_SHIFT = 32
  }
}
