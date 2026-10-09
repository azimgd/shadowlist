package com.shadowlist.kit

import android.content.Context
import android.content.res.Configuration
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Bundle
import android.os.Parcelable
import android.os.SystemClock
import android.util.AttributeSet
import android.view.Gravity
import android.view.HapticFeedbackConstants
import android.view.Menu
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.PopupMenu
import android.widget.TextView
import androidx.core.view.NestedScrollingChild3
import androidx.core.view.NestedScrollingChildHelper
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/*
 * A virtualized list on the shadowlist core. Rows are placed by the core, which keeps the
 * visible content still while rows are measured, inserted or removed. Measurement, offset
 * corrections and mounting all happen in one layout pass on the UI thread. A scroll frame
 * that stays inside the core's offset band does no core work at all. Sizes are in pixels.
 *
 * Item indices are the public indices. In a list with sections the core places rows: section
 * headers, items and section footers. The list converts between the two at its edges.
 */
open class ShadowListKitListView @JvmOverloads constructor(
  context: Context,
  attrs: AttributeSet? = null,
) : ViewGroup(context, attrs), NestedScrollingChild3 {

  interface DataSource {
    fun numberOfItems(listView: ShadowListKitListView): Int

    /*
     * A stable identity for the row. Sizes, cells and the scroll position follow keys across
     * reloadData. A prepend or an insert above keeps what is on screen in place.
     * A row whose content changed under the same key needs reloadItems.
     */
    fun keyForItem(listView: ShadowListKitListView, index: Int): String

    fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell

    /*
     * Update a shown cell for a payload given to reloadItems without a new cell. Return false
     * to have the row reloaded in full instead.
     */
    fun reconfigureCell(listView: ShadowListKitListView, cell: ShadowListKitListCell, index: Int, payload: Any?): Boolean = false
  }

  /*
   * Implemented by a data source that knows row sizes without a view, like from a layout
   * precomputed off the UI thread. Otherwise every row is measured once through its cell.
   */
  interface Sizing {
    fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int
  }

  /*
   * Implemented by a data source that groups its items in sections. numberOfItems is then not
   * read. Item indices still run across all sections, the same as without sections.
   * A section has a header when titleForHeaderInSection returns a title, an empty one too, and a
   * footer the same way. The list shows the title in a plain cell unless cellForHeaderInSection
   * gives one. Sizes come from sizeForHeaderInSection when it returns 0 or more, otherwise the
   * cell is measured. keyForSection gives the header and footer their identity, by default the
   * key of the section's first item. sectionIndexTitles are shown along the trailing edge and a
   * title scrolls to sectionForSectionIndexTitle, by default the section at its position.
   */
  interface Sections {
    fun numberOfSections(listView: ShadowListKitListView): Int
    fun numberOfItemsInSection(listView: ShadowListKitListView, section: Int): Int
    fun keyForSection(listView: ShadowListKitListView, section: Int): String? = null
    fun titleForHeaderInSection(listView: ShadowListKitListView, section: Int): String? = null
    fun titleForFooterInSection(listView: ShadowListKitListView, section: Int): String? = null
    fun cellForHeaderInSection(listView: ShadowListKitListView, section: Int): ShadowListKitListCell? = null
    fun cellForFooterInSection(listView: ShadowListKitListView, section: Int): ShadowListKitListCell? = null
    fun sizeForHeaderInSection(listView: ShadowListKitListView, section: Int, crossSize: Int): Int = -1
    fun sizeForFooterInSection(listView: ShadowListKitListView, section: Int, crossSize: Int): Int = -1
    fun sectionIndexTitles(listView: ShadowListKitListView): List<String>? = null
    fun sectionForSectionIndexTitle(listView: ShadowListKitListView, title: String, index: Int): Int = index
  }

  /*
   * Implemented by a data source whose items change content under the same key. The value
   * changes with the content, like a revision or a hash. applyChanges reloads the rows whose
   * value changed.
   */
  interface ContentVersions {
    fun contentVersionForItem(listView: ShadowListKitListView, index: Int): Long
  }

  /*
   * Hears which items the list will soon show, to load what their cells need ahead. Prefetched
   * items are the ones in the core's measured window, an overscan past the viewport, that have
   * no cell yet. A prefetched item that leaves the window before it shows is cancelled.
   */
  interface PrefetchDataSource {
    fun prefetchItems(listView: ShadowListKitListView, indices: IntArray)
    fun cancelPrefetchingForItems(listView: ShadowListKitListView, indices: IntArray) {}
  }

  interface Delegate {
    fun willDisplayCell(listView: ShadowListKitListView, cell: ShadowListKitListCell, index: Int) {}
    fun didEndDisplayingCell(listView: ShadowListKitListView, cell: ShadowListKitListCell, index: Int) {}
    fun didSelectItem(listView: ShadowListKitListView, index: Int) {}
    fun didDeselectItem(listView: ShadowListKitListView, index: Int) {}
    fun shouldSelectItem(listView: ShadowListKitListView, index: Int): Boolean = true
    fun shouldHighlightItem(listView: ShadowListKitListView, index: Int): Boolean = true
    fun didScroll(listView: ShadowListKitListView) {}

    /*
     * The list came to rest after a touch, a fling or an animated scroll, on a row edge when
     * snapToItem is set.
     */
    fun didEndScrolling(listView: ShadowListKitListView) {}

    /*
     * Whether a row can be picked up when reorderEnabled is set. Every row can by default.
     */
    fun canMoveItem(listView: ShadowListKitListView, index: Int): Boolean = true

    /*
     * A held row was dropped at another index. Move the item in the data, the list reads the
     * data again right after and keeps the dropped row where it was let go.
     */
    fun moveItem(listView: ShadowListKitListView, sourceIndex: Int, destinationIndex: Int) {}

    /*
     * The scroll position came within startReachedThreshold or endReachedThreshold of an edge.
     */
    fun didReachStart(listView: ShadowListKitListView) {}
    fun didReachEnd(listView: ShadowListKitListView) {}

    /*
     * Actions behind a row swiped from its leading or trailing side, or null for none.
     */
    fun leadingSwipeActionsForItem(listView: ShadowListKitListView, index: Int): ShadowListKitSwipeActionsConfiguration? = null
    fun trailingSwipeActionsForItem(listView: ShadowListKitListView, index: Int): ShadowListKitSwipeActionsConfiguration? = null

    /*
     * Fill the menu for touching and holding a row and return true, or false for none. A row
     * that can also be reordered lifts on the hold. Letting go without moving it shows the menu.
     */
    fun contextMenuForItem(listView: ShadowListKitListView, index: Int, menu: Menu): Boolean = false

    /*
     * Whether the separator below an item shows when showsSeparators is set. Separators only
     * go between items of one section.
     */
    fun showsSeparatorAfterItem(listView: ShadowListKitListView, index: Int): Boolean = true

    /*
     * The reader pulled to refresh with refreshEnabled. Set refreshing to false when done.
     */
    fun didBeginRefreshing(listView: ShadowListKitListView) {}
  }

  var dataSource: DataSource? = null
  var delegate: Delegate? = null
  var prefetchDataSource: PrefetchDataSource? = null

  /*
   * A chat style list that opens at its end and keeps the end in view while the reader is
   * there. Rows stay in data order, oldest first.
   */
  var inverted = false
    set(value) { field = value; sendSettings() }

  /*
   * With inverted, rows appended while the reader rests at the end scroll into view, like an
   * assistant reply. Off by default, which keeps the visible rows in place.
   */
  var followAppends = false
    set(value) { field = value; sendSettings() }

  var horizontal = false
    set(value) { field = value; sendSettings() }

  /*
   * Columns of a grid. Row i goes into column i % numberOfColumns and each column stacks its
   * own rows. Section headers take a column slot like any row, full width rows are not supported.
   */
  var numberOfColumns = 1
    set(value) { field = max(1, value); sendSettings() }

  /*
   * Size along the scroll axis assumed for rows not measured yet, in pixels.
   */
  var estimatedItemSize = 120 * resources.displayMetrics.density
    set(value) { field = value; sendSettings() }

  /*
   * How far past the viewport rows are measured, in viewport sizes. Default 1.
   */
  var overscan = 1.0
    set(value) { field = value; sendSettings() }

  /*
   * How far past the viewport cells are mounted, in viewport sizes. Default 0.5.
   */
  var mountOverscan = 0.5
    set(value) { field = value; invalidateFrame() }

  /*
   * Distances to an edge, in viewport sizes, that fire the reached callbacks. Default 1.
   */
  var startReachedThreshold = 1.0
    set(value) { field = value; sendSettings() }
  var endReachedThreshold = 1.0
    set(value) { field = value; sendSettings() }

  /*
   * Rest the scroll position on a row edge. Alignment 0 start, 1 center, 2 end.
   */
  var snapToItem = false
    set(value) { field = value; sendSettings() }
  var snapAlignment = 0
    set(value) { field = value; sendSettings() }

  /*
   * Touch and hold a row, then drag it to a new place. Other rows slide aside, and the list
   * scrolls when the row is held near an edge. Works in grids too. In a list with sections a
   * row stays in its section.
   */
  var reorderEnabled = false

  /*
   * Animate insertItems, deleteItems, reloadData, batches and applyChanges through itemAnimator:
   * new rows fade in, removed rows fade out and rows that stay slide from where they were to
   * where they are. The visible content stays anchored the same as without animations. Off by
   * default.
   */
  var animatesChanges = false

  var itemAnimator: ShadowListKitItemAnimator = ShadowListKitDefaultItemAnimator()

  /*
   * Items that stick to the top of the viewport once scrolled past.
   */
  var stickyIndices: IntArray = IntArray(0)
    set(value) {
      field = value.sortedArray()
      updateStickyRows()
      invalidateFrame()
    }

  /*
   * Every section header sticks to the top of the viewport once scrolled past.
   */
  var stickySectionHeaders = false
    set(value) {
      field = value
      updateStickyRows()
      invalidateFrame()
    }

  /*
   * Views before the first row and after the last. Their size along the scroll axis comes
   * from measuring with an unspecified size. Call requestLayout on the list after one changes.
   */
  var headerView: View? = null
    set(value) { replaceTemplate(field, value); field = value }
  var footerView: View? = null
    set(value) { replaceTemplate(field, value); field = value }

  /*
   * A tap selects a row when allowsSelection is set, the default. A tap on a selected row of a
   * list with allowsMultipleSelection deselects it. Selection follows keys.
   */
  var allowsSelection = true
    set(value) {
      field = value
      if (!value) selection.clear()
    }
  var allowsMultipleSelection = false
    set(value) {
      field = value
      if (!value && selection.count > 1) selection.clear()
    }

  /*
   * Passed to the cells. Turns swipe actions and reordering off.
   */
  var editing: Boolean
    get() = editingState
    set(value) = setEditing(value, false)

  /*
   * Pull to refresh with a spinner the list draws. refreshing shows it spinning.
   */
  var refreshEnabled = false
  var refreshing: Boolean
    get() = refresh.refreshing
    set(value) = refresh.setRefreshing(value)

  /*
   * Lines between items, drawn over the trailing edge of each item's cell. Insets are along the
   * cross axis from its start and end. Thickness defaults to one pixel.
   */
  var showsSeparators = false
    set(value) { field = value; invalidate() }
  var separatorColor = Color.argb(0x4a, 0x3c, 0x3c, 0x43)
    set(value) { field = value; separatorPaint.color = value; invalidate() }
  var separatorInsetStart = (16 * resources.displayMetrics.density).roundToInt()
    set(value) { field = value; invalidate() }
  var separatorInsetEnd = 0
    set(value) { field = value; invalidate() }
  var separatorThickness = 1
    set(value) { field = max(1, value); invalidate() }

  /*
   * Created on first use and destroyed when the list detaches. A list attached again gets a
   * new core with the same settings, keys and sections. Rows are measured again. A list that
   * never attaches leaves its core to ShadowListKitCore's reclaimer, which frees the peer once the list is
   * collected.
   */
  private var coreOrNull: ShadowListKitCore? = null
  internal val core: ShadowListKitCore
    get() = coreOrNull ?: createCore()

  /*
   * The core when it exists, for work that has nothing to do without one.
   */
  internal val liveCore: ShadowListKitCore? get() = coreOrNull

  /*
   * Created on first use. View's constructor can ask for nested scrolling state before the
   * fields here exist.
   */
  private var nestedHelper: NestedScrollingChildHelper? = null
  private val nested: NestedScrollingChildHelper
    get() = nestedHelper ?: NestedScrollingChildHelper(this).also {
      it.isNestedScrollingEnabled = true
      nestedHelper = it
    }

  internal val changes = ShadowListKitChangeAnimator(this)
  private val gesture = ShadowListKitScrollGesture(this)
  private val drag = ShadowListKitDragController(this)
  internal val swipe = ShadowListKitSwipeController(this)
  internal val refresh = ShadowListKitRefreshIndicator(this)
  private val sectionIndex = ShadowListKitSectionIndex(this)
  internal val selection = ShadowListKitSelection(this)

  private val data = ShadowListKitListData(this)

  /*
   * Row keys in data order, the same list the core holds, and the item count. Both live in
   * ShadowListKitListData.
   */
  internal val keys: ArrayList<String> get() = data.keys
  internal val itemCount: Int get() = data.itemCount

  /*
   * Mounted cells by key. A cell follows its key across inserts above it.
   */
  internal val mounted = HashMap<String, ShadowListKitListCell>()
  private var mountGeneration = 0L

  /*
   * What the last mount pass covered. The pass is skipped while all of it holds.
   */
  private var mountedLow = -1
  private var mountedHigh = -1
  private var mountedSticky = -1
  private var mountedGeometry = -1.0
  private var mountedStructure = 0
  private var structureVersion = 0

  private val cellFactories = HashMap<String, (Context) -> ShadowListKitListCell>()
  private val reusePool = HashMap<String, ArrayList<ShadowListKitListCell>>()

  /*
   * The rows the core keeps measured and their frames, four values each.
   */
  private var windowLow = -1
  private var windowHigh = -1
  private var windowFrames = DoubleArray(256)
  private var windowGeometry = -1.0
  private val scratchFrame = DoubleArray(4)

  private var stickyCell: ShadowListKitListCell? = null

  /*
   * The sticky rows: the sticky items' rows and the section headers, sorted.
   */
  private var stickyRows = IntArray(0)

  /*
   * Leading edge and extent of every sticky row, copied from the core when the geometry
   * changes. Pinning reads only these on a scroll frame.
   */
  private var stickyFrames = DoubleArray(0)
  private var stickyGeometry = -1.0
  private var frameGeometry = 0.0

  /*
   * Child positions of the views drawn last, see dispatchDraw.
   */
  private var liftedLow = -1
  private var liftedHigh = -1
  private var liftedTop = -1

  /*
   * The band of offsets where the core has nothing to do.
   */
  private var bandLow = 1.0
  private var bandHigh = 0.0
  private var needsFrame = true

  /*
   * Geometry the core last ran with.
   */
  internal var windowAlong = 0
    private set
  internal var windowCross = 0
    private set
  private var headerSize = 0
  private var footerSize = 0
  internal var contentAlong = 0
    private set

  /*
   * The last offset seen, to tell the user's scrolling from our own writes.
   */
  private var previousOffset = 0
  private var userScrolled = false

  private var inLayoutPass = false
  private var settleScheduled = false
  private var reachedStart = false
  private var reachedEnd = false

  private var editingState = false

  private var pendingAnchor: ShadowListKitAnchorState? = null
  private val decorations = ArrayList<ShadowListKitItemDecoration>()
  private val separatorPaint = Paint().apply { color = separatorColor }
  private val separatorRect = RectF()

  internal val density = resources.displayMetrics.density

  init {
    clipChildren = true
    isChildrenDrawingOrderEnabled = true
    isVerticalScrollBarEnabled = true
    setWillNotDraw(false)
  }

  override fun onDetachedFromWindow() {
    super.onDetachedFromWindow()
    gesture.stop()
    gesture.releaseTracker()
    drag.end(false)
    swipe.close(false)
    swipe.releaseTracker()
    selection.cancelHighlight()
    destroyCore()
  }

  // region Axis

  internal fun along(x: Float, y: Float): Float = if (horizontal) x else y
  internal fun cross(x: Float, y: Float): Float = if (horizontal) y else x

  /*
   * The padding before the rows along and across the scroll axis. Rows start there.
   */
  internal val leadingPadding: Int get() = if (horizontal) paddingLeft else paddingTop
  internal val crossPadding: Int get() = if (horizontal) paddingTop else paddingLeft
  private val trailingPadding: Int get() = if (horizontal) paddingRight else paddingBottom

  /*
   * The scroll offset along the axis, in pixels.
   */
  internal val offset: Int get() = if (horizontal) scrollX else scrollY

  internal val maxOffset: Int get() = max(0, contentAlong - windowAlong)

  /*
   * A point in the list's own coordinates as a position in the content the core places rows
   * in, along and across the scroll axis.
   */
  internal fun contentAlongAt(x: Float, y: Float): Float = along(x, y) + offset - leadingPadding
  internal fun contentCrossAt(x: Float, y: Float): Float = cross(x, y) - crossPadding

  /*
   * A point in the list's own coordinates as a position in the padded window along the axis.
   */
  internal fun windowAlongAt(x: Float, y: Float): Float = along(x, y) - leadingPadding

  /*
   * Move to an offset the list asked for itself. Only a move made for the user, like the drag
   * auto scroll, counts as the user's scrolling.
   */
  internal fun writeOffset(value: Int, byUser: Boolean = false) {
    previousOffset = value
    if (byUser) userScrolled = true
    if (horizontal) super.scrollTo(value, 0) else super.scrollTo(0, value)
  }

  /*
   * The bench and other callers move the list through scrollTo. Keep them inside the content.
   */
  override fun scrollTo(x: Int, y: Int) {
    val along = if (horizontal) x else y
    val clamped = if (gesture.isTracking) along else min(max(along, 0), maxOffset)
    if (horizontal) super.scrollTo(clamped, 0) else super.scrollTo(0, clamped)
  }

  private fun scrollPhase(): Int = when {
    gesture.isTracking -> ShadowListKitCore.PHASE_DRAGGING
    gesture.isFlinging -> ShadowListKitCore.PHASE_SETTLING
    else -> ShadowListKitCore.PHASE_IDLE
  }

  // endregion

  // region Properties

  private fun sendSettings() {
    coreOrNull?.let(::applySettings)
    invalidateFrame()
  }

  private fun applySettings(target: ShadowListKitCore) {
    target.setSettings(estimatedItemSize.toDouble(), overscan, startReachedThreshold, endReachedThreshold,
      numberOfColumns, inverted, followAppends, horizontal, snapToItem, snapAlignment)
  }

  /*
   * A new core with this list's settings, sticky rows, sections and keys. Every copy of its
   * output here is dropped and the next layout pass runs it.
   */
  private fun createCore(): ShadowListKitCore {
    val created = ShadowListKitCore(::measureItem)
    coreOrNull = created
    applySettings(created)
    if (stickyRows.isNotEmpty()) created.setStickyIndices(stickyRows)
    created.setSections(data.itemCount, data.sectionCounts, data.sectionFlags)
    if (keys.isNotEmpty()) created.replaceKeys(0, 0, keys, 0, keys.size)
    windowLow = -1
    windowHigh = -1
    windowGeometry = -1.0
    stickyGeometry = -1.0
    bandLow = 1.0
    bandHigh = 0.0
    mountedLow = -1
    needsFrame = true
    ++structureVersion
    return created
  }

  private fun destroyCore() {
    coreOrNull?.destroy()
    coreOrNull = null
  }

  private fun replaceTemplate(previous: View?, next: View?) {
    if (previous != null) removeView(previous)
    if (next != null) addView(next, 0)
    invalidateFrame()
  }

  /*
   * Run the core on the next layout pass even inside the band.
   */
  internal fun invalidateFrame() {
    needsFrame = true
    requestLayout()
  }

  fun setEditing(editing: Boolean, animated: Boolean) {
    if (editing) swipe.close(animated)
    for (cell in mounted.values) if (cell.editing != editing) cell.setEditing(editing, animated)
    editingState = editing
  }

  fun addItemDecoration(decoration: ShadowListKitItemDecoration) {
    decorations.add(decoration)
    invalidate()
  }

  fun removeItemDecoration(decoration: ShadowListKitItemDecoration) {
    decorations.remove(decoration)
    invalidate()
  }

  // endregion

  // region Cells

  fun registerCell(identifier: String, factory: (Context) -> ShadowListKitListCell) {
    cellFactories[identifier] = factory
  }

  @Suppress("UNCHECKED_CAST")
  fun <T : ShadowListKitListCell> dequeueReusableCell(identifier: String): T {
    val pool = reusePool[identifier]
    if (pool != null && pool.isNotEmpty()) {
      val cell = pool.removeAt(pool.size - 1)
      cell.prepareForReuse()
      return cell as T
    }
    val factory = cellFactories[identifier] ?: { context: Context -> ShadowListKitListCell(context, identifier) }
    val cell = factory(context)
    cell.visibility = INVISIBLE
    addViewInLayout(cell, -1, generateDefaultLayoutParams(), true)
    return cell as T
  }

  internal fun recycleCell(cell: ShadowListKitListCell) {
    val index = cell.index
    swipe.cellWillRecycle(cell)
    drag.cellWillRecycle(cell)
    cell.animate().cancel()
    cell.alpha = 1f
    cell.translationX = 0f
    cell.translationY = 0f
    cell.visibility = INVISIBLE
    cell.index = ShadowListKitListCell.NO_INDEX
    cell.row = ShadowListKitListCell.NO_INDEX
    if (cell.highlighted) cell.setHighlighted(false, false)
    if (cell.isSelected) cell.setSelected(false, false)
    if (index >= 0) delegate?.didEndDisplayingCell(this, cell, index)
    val identifier = cell.reuseIdentifier
    if (identifier == null) {
      removeViewInLayout(cell)
      return
    }
    reusePool.getOrPut(identifier) { ArrayList() }.add(cell)
  }

  /*
   * The cell of a row: an item's from the data source, or a section header or footer.
   */
  private fun makeCell(row: Int): ShadowListKitListCell {
    val item = itemForRow(row)
    val cell = if (item >= 0) requireNotNull(dataSource).cellForItem(this, item) else sectionCell(row)
    if (cell.parent !== this) {
      addViewInLayout(cell, -1, generateDefaultLayoutParams(), true)
    }
    cell.row = row
    cell.index = item
    return cell
  }

  private fun sectionCell(row: Int): ShadowListKitListCell {
    val place = core.placeOfRow(row)
    val section = place shr ShadowListKitCore.ROW_KIND_BITS
    val footer = (place and ShadowListKitCore.ROW_KIND_MASK) == ShadowListKitCore.ROW_FOOTER
    val sections = dataSource as Sections
    val custom = if (footer) sections.cellForFooterInSection(this, section) else sections.cellForHeaderInSection(this, section)
    if (custom != null) return custom
    val identifier = if (footer) ShadowListKitSectionTitleCell.FOOTER else ShadowListKitSectionTitleCell.HEADER
    if (identifier !in cellFactories) registerCell(identifier) { ShadowListKitSectionTitleCell(it, identifier) }
    val cell = dequeueReusableCell<ShadowListKitSectionTitleCell>(identifier)
    cell.title = (if (footer) sections.titleForFooterInSection(this, section) else sections.titleForHeaderInSection(this, section)) ?: ""
    return cell
  }

  internal fun keyAt(index: Int): String? = keys.getOrNull(index)

  /*
   * The mounted cell of a row, or null.
   */
  private fun mountedCell(row: Int): ShadowListKitListCell? = keyAt(row)?.let { mounted[it] }

  override fun generateDefaultLayoutParams(): LayoutParams =
    LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT)

  // endregion

  // region Data

  /*
   * Read the row count and keys again. Rows keep their sizes and cells by key, and the
   * visible content stays in place. Cells of surviving keys are not configured again.
   * Only the keys between the unchanged rows at both ends go to the core.
   */
  fun reloadData() = data.reloadData()

  /*
   * Rows were inserted at these positions of the new data, which the data source already
   * reflects. Only the new keys are read. A list with sections reads everything again. An
   * insert past the end goes at the end.
   */
  fun insertItems(indices: IntArray) = data.insertItems(indices)

  /*
   * Rows were deleted at these positions of the old data. A delete past the end is dropped.
   */
  fun deleteItems(indices: IntArray) = data.deleteItems(indices)

  /*
   * The rows' content changed under the same keys. Visible cells are configured again and
   * every listed row is measured again. With a payload the data source's reconfigureCell can
   * update a shown cell instead.
   */
  @JvmOverloads
  fun reloadItems(indices: IntArray, payload: Any? = null) = data.reloadItems(indices, payload)

  /*
   * An item moved, after the data source reflects it.
   */
  fun moveItem(index: Int, newIndex: Int) = data.moveItem(index, newIndex)

  /*
   * Inserts, deletes, moves and reloads made in updates land together in one layout and one
   * animation, the way UITableView takes them: deletes, reloads and move sources are indices in
   * the data before, inserts and move destinations in the data after. A batch that does not add
   * up reloads everything. completion runs once the change animation ended.
   */
  @JvmOverloads
  fun performBatchUpdates(updates: () -> Unit, completion: ((finished: Boolean) -> Unit)? = null) =
    data.performBatchUpdates(updates, completion)

  /*
   * The data source already shows the new data. Read every key, work out the inserts, deletes
   * and moves against the keys held with the core's diffKeys, reload the rows whose content
   * version changed, and return what changed. Animates like any change with animatesChanges.
   */
  fun applyChanges(): ShadowListKitListChanges = data.applyChanges()

  /*
   * The data changed. Sticky rows follow the sections, the selection drops removed rows and a
   * waiting saved position lands once its row is there.
   */
  internal fun structureChanged() {
    ++structureVersion
    // An open row closes. One swiped all the way stays out while its removal runs.
    swipe.cell?.let { if (!swipe.isSwipedOut(it)) swipe.close(false) }
    if (stickyIndices.isNotEmpty() || stickySectionHeaders) updateStickyRows()
    selection.dropRemovedKeys(keys)
    restorePendingAnchor()
    invalidateFrame()
  }

  // endregion

  // region Sections

  internal fun itemForRow(row: Int): Int {
    val items = data.rowItems ?: return if (row in keys.indices) row else -1
    return if (row in items.indices) items[row] else -1
  }

  internal fun rowForItem(item: Int): Int {
    if (item < 0 || item >= itemCount) return -1
    return if (isSectioned) core.rowForItem(item) else item
  }

  internal val isSectioned: Boolean get() = data.sectionCounts != null

  val numberOfSections: Int get() = data.sectionCounts?.size ?: 1

  /*
   * The section of an item, or -1.
   */
  fun sectionForItem(index: Int): Int {
    if (index < 0 || index >= itemCount) return -1
    return if (isSectioned) core.sectionForItem(index) else 0
  }

  /*
   * The item index a section's items start at, or -1.
   */
  fun firstItemIndexInSection(section: Int): Int {
    if (!isSectioned) return if (section == 0) 0 else -1
    return core.firstItemInSection(section)
  }

  /*
   * The frame of a section's header in the list's scrolled coordinates, or null.
   */
  fun rectForHeaderInSection(section: Int): RectF? {
    if (!isSectioned) return null
    return rowRectF(core.headerRow(section))
  }

  fun scrollToSection(section: Int, animated: Boolean = false) {
    val row = if (isSectioned) core.firstRowInSection(section) else if (section == 0 && keys.isNotEmpty()) 0 else -1
    if (row >= 0) scrollToRow(row, 0.0, animated)
  }

  private fun updateStickyRows() {
    val sorted = core.stickyRows(stickyIndices, stickySectionHeaders)
    if (sorted.contentEquals(stickyRows)) return
    stickyRows = sorted
    coreOrNull?.setStickyIndices(sorted)
    stickyGeometry = -1.0
    mountedLow = -1
  }

  internal fun reloadSectionIndex() {
    val sections = dataSource as? Sections
    val titles = if (horizontal) null else sections?.sectionIndexTitles(this)
    sectionIndex.titles = titles ?: emptyList()
    sectionIndex.onSelect = { index ->
      val title = sectionIndex.titles.getOrNull(index)
      if (title != null && sections != null) scrollToSection(sections.sectionForSectionIndexTitle(this, title, index))
    }
    invalidate()
  }

  // endregion

  // region Layout

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(
      getDefaultSize(suggestedMinimumWidth, widthMeasureSpec),
      getDefaultSize(suggestedMinimumHeight, heightMeasureSpec))
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    layoutPass()
  }

  /*
   * Children asking for layout while the pass binds them get measured by the pass itself.
   * Asking the whole window for a traversal would only repeat it.
   */
  override fun requestLayout() {
    if (!inLayoutPass) super.requestLayout()
  }

  override fun onScrollChanged(l: Int, t: Int, oldl: Int, oldt: Int) {
    super.onScrollChanged(l, t, oldl, oldt)
    layoutPass()
    delegate?.didScroll(this)
  }

  /*
   * One pass runs the core when the offset left the band, then mounts. Everything lands
   * before the frame is drawn.
   */
  internal fun layoutPass() {
    if (inLayoutPass) return
    inLayoutPass = true
    val geometryChanged = readGeometry()
    trackUserScroll()
    if (windowAlong > 0 && windowCross > 0) {
      if (needsFrame || geometryChanged || !inBand(offset)) runPasses()
      layoutTemplates()
      mountCells()
      layoutSticky()
      if (hasHeldRow) drag.applyShifts(false)
      changes.run()
      swipe.layout()
    }
    inLayoutPass = false
    dispatchReached()
  }

  private fun inBand(value: Int): Boolean = bandLow <= value && value <= bandHigh

  /*
   * Read the viewport and template sizes. Returns whether any changed since the last pass.
   */
  private fun readGeometry(): Boolean {
    // Padding works like content insets: the window the core sees is inside it.
    val along = (if (horizontal) width - paddingLeft - paddingRight else height - paddingTop - paddingBottom).coerceAtLeast(0)
    val cross = (if (horizontal) height - paddingTop - paddingBottom else width - paddingLeft - paddingRight).coerceAtLeast(0)
    if (cross != windowCross && windowCross > 0) resetKeepingPosition()
    val header = templateSize(headerView, cross)
    val footer = templateSize(footerView, cross)
    val changed = along != windowAlong || cross != windowCross || header != headerSize || footer != footerSize
    windowAlong = along
    windowCross = cross
    headerSize = header
    footerSize = footer
    return changed
  }

  private fun trackUserScroll() {
    val current = offset
    if (abs(current - previousOffset) < 1) return
    if (scrollPhase() != ShadowListKitCore.PHASE_IDLE) userScrolled = true
    previousOffset = current
  }

  private fun templateSize(view: View?, cross: Int): Int {
    if (view == null || cross <= 0) return 0
    val crossSpec = MeasureSpec.makeMeasureSpec(cross, MeasureSpec.EXACTLY)
    val alongSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
    if (view.isLayoutRequested || (if (horizontal) view.measuredHeight else view.measuredWidth) != cross) {
      if (horizontal) view.measure(alongSpec, crossSpec) else view.measure(crossSpec, alongSpec)
    }
    return if (horizontal) view.measuredWidth else view.measuredHeight
  }

  private fun layoutTemplates() {
    headerView?.let { placeTemplate(it, 0, headerSize) }
    footerView?.let { placeTemplate(it, core.footerStart(footerSize.toDouble()).roundToInt(), footerSize) }
  }

  private fun placeTemplate(view: View, start: Int, size: Int) {
    val left = paddingLeft
    val top = paddingTop
    if (horizontal) layoutIfMoved(view, left + start, top, left + start + size, top + windowCross)
    else layoutIfMoved(view, left, top + start, left + windowCross, top + start + size)
  }

  private fun layoutIfMoved(view: View, left: Int, top: Int, right: Int, bottom: Int) {
    if (view.left != left || view.top != top || view.right != right || view.bottom != bottom || view.isLayoutRequested) {
      view.layout(left, top, right, bottom)
    }
  }

  /*
   * Run the core until the window is measured and any correction landed. Everything happens
   * before the frame is drawn. The reader never sees an estimate or a correction.
   */
  private fun runPasses() {
    val pass = core.pass
    pass[ShadowListKitCore.PASS_OFFSET] = offset.toDouble()
    pass[ShadowListKitCore.PASS_WINDOW_ALONG] = windowAlong.toDouble()
    pass[ShadowListKitCore.PASS_WINDOW_CROSS] = windowCross.toDouble()
    pass[ShadowListKitCore.PASS_HEADER_SIZE] = headerSize.toDouble()
    pass[ShadowListKitCore.PASS_FOOTER_SIZE] = footerSize.toDouble()
    pass[ShadowListKitCore.PASS_PHASE] = scrollPhase().toDouble()
    pass[ShadowListKitCore.PASS_USER_SCROLLED] = if (userScrolled) 1.0 else 0.0
    pass[ShadowListKitCore.PASS_TRACKING] = if (gesture.isTracking) 1.0 else 0.0
    core.runPasses()
    userScrolled = false
    needsFrame = false
    applyPassResult(pass)
  }

  /*
   * The content size goes first. The offset write is then inside the scroll range.
   */
  private fun applyPassResult(pass: DoubleArray) {
    contentAlong = pass[ShadowListKitCore.PASS_OUT_CONTENT].roundToInt()
    val target = pass[ShadowListKitCore.PASS_OUT_OFFSET].roundToInt()
    if (target != offset) writeOffset(target)
    bandLow = pass[ShadowListKitCore.PASS_OUT_BAND_LOW]
    bandHigh = pass[ShadowListKitCore.PASS_OUT_BAND_HIGH]
    reachedStart = reachedStart || pass[ShadowListKitCore.PASS_OUT_REACHED_START] != 0.0
    reachedEnd = reachedEnd || pass[ShadowListKitCore.PASS_OUT_REACHED_END] != 0.0
    frameGeometry = pass[ShadowListKitCore.PASS_OUT_GEOMETRY]
    copyWindow(pass)
    if (pass[ShadowListKitCore.PASS_OUT_SETTLING] != 0.0) scheduleSettleFrame()
  }

  /*
   * Keep the window's frames on this side. Mount passes inside the band read only these.
   */
  private fun copyWindow(pass: DoubleArray) {
    val low = pass[ShadowListKitCore.PASS_OUT_WINDOW_LOW].toInt()
    val high = pass[ShadowListKitCore.PASS_OUT_WINDOW_HIGH].toInt()
    val geometry = pass[ShadowListKitCore.PASS_OUT_GEOMETRY]
    if (low == windowLow && high == windowHigh && geometry == windowGeometry) return
    windowLow = low
    windowHigh = high
    windowGeometry = geometry
    if (low < 0) return
    val needed = (high - low + 1) * 4
    if (windowFrames.size < needed) windowFrames = DoubleArray(needed * 2)
    core.copyRowRects(low, high, windowFrames)
  }

  /*
   * A correction waits for one more report. Give it the next display frame, once.
   */
  private fun scheduleSettleFrame() {
    needsFrame = true
    if (settleScheduled) return
    settleScheduled = true
    postOnAnimation {
      settleScheduled = false
      // A list detached meanwhile dropped its core. The next attach lays out again.
      if (isAttachedToWindow) layoutPass()
    }
  }

  /*
   * Drop all sizes after a cross size change and hold the first visible row.
   */
  private fun resetKeepingPosition() {
    core.resetKeepingPosition()
    windowLow = -1
    windowHigh = -1
    windowGeometry = -1.0
    bandLow = 1.0
    bandHigh = 0.0
    needsFrame = true
    ++structureVersion
  }

  // endregion

  // region Mounting

  private fun windowLeading(index: Int): Double {
    val at = (index - windowLow) * 4
    return if (horizontal) windowFrames[at] else windowFrames[at + 1]
  }

  private fun windowExtent(index: Int): Double {
    val at = (index - windowLow) * 4
    return if (horizontal) windowFrames[at + 2] else windowFrames[at + 3]
  }

  private fun overlaps(index: Int, viewLow: Double, viewHigh: Double): Boolean {
    val start = windowLeading(index)
    return start + windowExtent(index) > viewLow && start < viewHigh
  }

  /*
   * Mount the cells of rows near the viewport and recycle the rest. Rows are matched by key,
   * which keeps the content of a cell that only moved. The range follows the core's
   * ListDriver::planMount with the padding as insets, run here over the copied frames. A
   * scroll frame makes no JNI call. The core test list_driver_mount_plan_reaches_into_the_insets
   * holds the rule this matches.
   */
  private fun mountCells() {
    if (windowLow < 0 || keys.isEmpty()) {
      unmountAll()
      return
    }
    // Rows under the padding show when clipToPadding is off. They are mounted too.
    val pad = windowAlong * mountOverscan
    val viewLow = offset - pad - leadingPadding
    val viewHigh = offset + windowAlong + pad + trailingPadding
    var low = -1
    var high = -1
    for (index in windowLow..windowHigh) {
      if (!overlaps(index, viewLow, viewHigh)) continue
      if (low < 0) low = index
      high = index
    }
    val sticky = activeStickyIndex(offset.toDouble())
    if (!mountNeeded(low, high, sticky)) return
    recordMount(low, high, sticky)
    mountPlan(low, high, sticky, viewLow, viewHigh)
    prefetchAround(low, high)
  }

  /*
   * Whether anything the last mount pass covered changed: the rows, the pinned header, the
   * geometry or the data. A drag mounts every pass.
   */
  private fun mountNeeded(low: Int, high: Int, sticky: Int): Boolean =
    low != mountedLow || high != mountedHigh || sticky != mountedSticky ||
      windowGeometry != mountedGeometry || structureVersion != mountedStructure || hasHeldRow

  private fun recordMount(low: Int, high: Int, sticky: Int) {
    mountedLow = low
    mountedHigh = high
    mountedSticky = sticky
    mountedGeometry = windowGeometry
    mountedStructure = structureVersion
  }

  private fun mountPlan(low: Int, high: Int, sticky: Int, viewLow: Double, viewHigh: Double) {
    val generation = ++mountGeneration
    if (low >= 0) {
      for (index in low..high) {
        // A masonry grid can hold rows inside the index range that are out of view.
        if (numberOfColumns > 1 && !overlaps(index, viewLow, viewHigh)) continue
        mountRow(index, generation)
      }
    }
    if (sticky in keys.indices) mountRow(sticky, generation)
    // The held row stays mounted wherever the finger takes it.
    val held = drag.heldIndex
    if (held in keys.indices) mountRow(held, generation)
    recycleStaleCells(generation)
  }

  private fun mountRow(index: Int, generation: Long) {
    val key = keys[index]
    var cell = mounted[key]
    val appearing = cell == null || cell.mountGeneration == 0L
    if (cell == null) {
      cell = makeCell(index)
      mounted[key] = cell
    }
    cell.row = index
    cell.index = itemForRow(index)
    cell.mountGeneration = generation
    placeCell(cell, index)
    if (appearing) {
      cell.visibility = VISIBLE
      applyState(cell, key)
      if (cell.index >= 0) delegate?.willDisplayCell(this, cell, cell.index)
    }
  }

  /*
   * A cell coming on screen shows its row's selection and the list's editing state.
   */
  private fun applyState(cell: ShadowListKitListCell, key: String) {
    val selected = cell.index >= 0 && selection.isSelected(key)
    if (cell.isSelected != selected) cell.setSelected(selected, false)
    if (cell.editing != editingState) cell.setEditing(editingState, false)
  }

  private fun recycleStaleCells(generation: Long) {
    val entries = mounted.entries.iterator()
    while (entries.hasNext()) {
      val entry = entries.next()
      val cell = entry.value
      if (cell.mountGeneration != generation) {
        entries.remove()
        // A removed row's cell fades out first, then goes back to the pool. One swiped out does not.
        if (swipe.isSwipedOut(cell) || !changes.fadeOut(entry.key, cell)) recycleCell(cell)
      }
    }
  }

  private fun unmountAll() {
    for (cell in mounted.values) recycleCell(cell)
    mounted.clear()
    mountedLow = -1
    mountedHigh = -1
  }

  /*
   * The row's frame from the window copy, or from the core for rows outside it.
   */
  private fun rowRect(index: Int, out: DoubleArray): Boolean {
    if (index in windowLow..windowHigh && windowLow >= 0) {
      System.arraycopy(windowFrames, (index - windowLow) * 4, out, 0, 4)
      return true
    }
    return core.rowRect(index, out)
  }

  private fun placeCell(cell: ShadowListKitListCell, index: Int) {
    if (!rowRect(index, scratchFrame)) return
    placeView(cell, scratchFrame[0], scratchFrame[1], scratchFrame[2], scratchFrame[3])
  }

  /*
   * Place a view at a frame. A view that only moved is offset without a layout.
   */
  internal fun placeView(view: View, x: Double, y: Double, w: Double, h: Double) {
    val left = x.roundToInt() + paddingLeft
    val top = y.roundToInt() + paddingTop
    val right = (x + w).roundToInt() + paddingLeft
    val bottom = (y + h).roundToInt() + paddingTop
    val width = right - left
    val height = bottom - top
    if (view.isLayoutRequested || view.width != width || view.height != height) {
      view.measure(
        MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
        MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
      view.layout(left, top, right, bottom)
      return
    }
    if (view.left != left) view.offsetLeftAndRight(left - view.left)
    if (view.top != top) view.offsetTopAndBottom(top - view.top)
  }

  internal fun addSwipeActionsView(view: View) {
    addViewInLayout(view, 0, generateDefaultLayoutParams(), true)
    invalidate()
  }

  internal fun removeSwipeActionsView(view: View) {
    removeViewInLayout(view)
    invalidate()
  }

  // endregion

  // region Prefetching

  /*
   * Tell the prefetch data source about items the measured window brought in that have no cell
   * yet, and the ones that left it unseen. One JNI call per mount pass, none on band frames.
   */
  private fun prefetchAround(low: Int, high: Int) {
    val prefetch = prefetchDataSource ?: return
    val packed = core.updatePrefetch(low, high)
    val prefetchCount = packed[0]
    val items = packed.copyOfRange(1, 1 + prefetchCount)
    val cancelCount = packed[1 + prefetchCount]
    val cancelled = packed.copyOfRange(2 + prefetchCount, 2 + prefetchCount + cancelCount)
    if (items.isNotEmpty()) prefetch.prefetchItems(this, items)
    if (cancelled.isNotEmpty()) prefetch.cancelPrefetchingForItems(this, cancelled)
  }

  // endregion

  // region Measurement

  /*
   * Called by the core for every row in its window that has no size yet.
   */
  private fun measureItem(row: Int, crossSize: Double): Double {
    val cross = crossSize.roundToInt()
    val item = itemForRow(row)
    if (item >= 0) {
      val sizing = dataSource as? Sizing
      if (sizing != null) return sizing.sizeForItem(this, item, cross).toDouble()
    } else {
      val sections = dataSource as? Sections
      val place = core.placeOfRow(row)
      if (sections != null && place >= 0) {
        val section = place shr ShadowListKitCore.ROW_KIND_BITS
        val size = if ((place and ShadowListKitCore.ROW_KIND_MASK) == ShadowListKitCore.ROW_FOOTER) sections.sizeForFooterInSection(this, section, cross)
          else sections.sizeForHeaderInSection(this, section, cross)
        if (size >= 0) return size.toDouble()
      }
    }
    return measureCell(row, cross).toDouble()
  }

  /*
   * Measure a row through its cell. The cell is mounted right after when the row is in view,
   * which rarely wastes the configure work.
   */
  private fun measureCell(row: Int, cross: Int): Int {
    val key = keys[row]
    val cell = mounted[key] ?: makeCell(row).also {
      it.mountGeneration = 0L
      mounted[key] = it
    }
    val crossSpec = MeasureSpec.makeMeasureSpec(cross, MeasureSpec.EXACTLY)
    val alongSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
    if (horizontal) cell.measure(alongSpec, crossSpec) else cell.measure(crossSpec, alongSpec)
    return if (horizontal) cell.measuredWidth else cell.measuredHeight
  }

  // endregion

  // region Sticky

  private fun refreshStickyFrames() {
    if (stickyGeometry == frameGeometry) return
    if (stickyFrames.size < stickyRows.size * 2) stickyFrames = DoubleArray(stickyRows.size * 2)
    if (core.copyStickyFrames(stickyFrames)) stickyGeometry = frameGeometry
  }

  /*
   * Position in stickyRows of the last sticky row starting at or above the offset, or -1. Rows
   * the core has not placed yet start at infinity and sort last. The core test
   * list_driver_sticky_frames_pin_the_same_header_as_the_driver holds the rule this and
   * stickyLeading match.
   */
  private fun activeStickyPosition(offset: Double): Int {
    if (stickyRows.isEmpty()) return -1
    refreshStickyFrames()
    var found = -1
    var low = 0
    var high = stickyRows.size
    while (low < high) {
      val mid = (low + high) ushr 1
      if (stickyFrames[mid * 2] <= offset) {
        found = mid
        low = mid + 1
      } else {
        high = mid
      }
    }
    return found
  }

  private fun activeStickyIndex(offset: Double): Int =
    activeStickyPosition(offset).let { if (it >= 0) stickyRows[it] else -1 }

  /*
   * Where the pinned header's leading edge goes, pushed up by the next header.
   */
  private fun stickyLeading(position: Int, offset: Double): Double {
    var pinned = max(stickyFrames[position * 2], offset)
    val next = position + 1
    if (next < stickyRows.size && stickyFrames[next * 2].isFinite()) {
      pinned = min(pinned, stickyFrames[next * 2] - stickyFrames[position * 2 + 1])
    }
    return pinned
  }

  /*
   * Pin the active section header and put the one it replaced back in its row.
   */
  private fun layoutSticky() {
    if (stickyRows.isEmpty()) return
    val position = activeStickyPosition(offset.toDouble())
    val active = if (position >= 0) stickyRows[position] else -1
    val cell = mountedCell(active)
    val previous = stickyCell
    if (previous != null && previous !== cell) unpinCell(previous)
    stickyCell = cell
    if (cell == null) return
    val pinned = stickyLeading(position, offset.toDouble())
    val extent = stickyFrames[position * 2 + 1]
    val cross = windowCross.toDouble()
    if (horizontal) placeView(cell, pinned, 0.0, extent, cross) else placeView(cell, 0.0, pinned, cross, extent)
  }

  private fun unpinCell(cell: ShadowListKitListCell) {
    if (cell.row in keys.indices) placeCell(cell, cell.row)
  }

  /*
   * The pinned header and the held row draw above the other rows. Their child positions are
   * found once per draw, ascending, -1 when absent. Decorations draw below and above the rows,
   * and the refresh spinner and section index over everything.
   */
  override fun dispatchDraw(canvas: Canvas) {
    for (decoration in decorations) decoration.onDraw(canvas, this)
    val sticky = stickyCell?.let { indexOfChild(it) } ?: -1
    val held = drag.heldCell?.let { indexOfChild(it) } ?: -1
    liftedLow = if (sticky >= 0 && held >= 0) min(sticky, held) else max(sticky, held)
    liftedHigh = if (sticky >= 0 && held >= 0) max(sticky, held) else -1
    // The held row draws last, above the pinned header.
    liftedTop = if (held >= 0) held else sticky
    super.dispatchDraw(canvas)
    for (decoration in decorations) decoration.onDrawOver(canvas, this)
    if (refresh.draw(canvas)) postInvalidateOnAnimation()
    sectionIndex.draw(canvas)
  }

  override fun getChildDrawingOrder(childCount: Int, drawingPosition: Int): Int {
    if (liftedLow < 0) return drawingPosition
    val liftedCount = if (liftedHigh >= 0) 2 else 1
    val rest = childCount - liftedCount
    if (drawingPosition == childCount - 1) return liftedTop
    if (drawingPosition >= rest) return if (liftedTop == liftedLow) liftedHigh else liftedLow
    var position = drawingPosition
    if (liftedLow <= position) ++position
    if (liftedHigh in 0..position) ++position
    return position
  }

  // endregion

  // region Separators

  /*
   * Each item's separator draws right after the item, which keeps a pinned header above the
   * separators of the rows under it.
   */
  override fun drawChild(canvas: Canvas, child: View, drawingTime: Long): Boolean {
    val more = super.drawChild(canvas, child, drawingTime)
    if (showsSeparators && child is ShadowListKitListCell && child.visibility == VISIBLE && child.index >= 0) {
      drawSeparator(canvas, child)
    }
    return more
  }

  private fun drawSeparator(canvas: Canvas, cell: ShadowListKitListCell) {
    val row = cell.row
    if (numberOfColumns > 1 || row < 0) return
    val follows = data.rowSeparators?.let { row < it.size && it[row] } ?: (row + 1 < keys.size)
    if (!follows || delegate?.showsSeparatorAfterItem(this, cell.index) == false) return
    val x = cell.left + cell.translationX
    val y = cell.top + cell.translationY
    // The line never runs backwards when the insets are wider than the cell.
    if (horizontal) {
      val length = max(0, cell.height - separatorInsetStart - separatorInsetEnd)
      separatorRect.set(x + cell.width - separatorThickness, y + separatorInsetStart, x + cell.width, y + separatorInsetStart + length)
    } else {
      val length = max(0, cell.width - separatorInsetStart - separatorInsetEnd)
      separatorRect.set(x + separatorInsetStart, y + cell.height - separatorThickness, x + separatorInsetStart + length, y + cell.height)
    }
    separatorPaint.alpha = (Color.alpha(separatorColor) * cell.alpha).roundToInt()
    canvas.drawRect(separatorRect, separatorPaint)
  }

  // endregion

  // region Events

  private fun dispatchReached() {
    if (reachedStart) {
      reachedStart = false
      delegate?.didReachStart(this)
    }
    if (reachedEnd) {
      reachedEnd = false
      delegate?.didReachEnd(this)
    }
  }

  /*
   * The visible cell under a point in the list's own coordinates.
   */
  internal fun cellAt(x: Float, y: Float): ShadowListKitListCell? {
    val contentX = x + scrollX
    val contentY = y + scrollY
    for (cell in mounted.values) {
      if (cell.visibility == VISIBLE && cell.row >= 0 &&
        contentX >= cell.left && contentX < cell.right && contentY >= cell.top && contentY < cell.bottom) {
        return cell
      }
    }
    return null
  }

  /*
   * The visible item cell under a point, not a section header or footer.
   */
  internal fun itemCellAt(x: Float, y: Float): ShadowListKitListCell? = cellAt(x, y)?.takeIf { it.index >= 0 }

  internal fun handleTap(x: Float, y: Float) {
    if (swipe.isOpen || swipe.closingTouch || hasHeldRow) return
    val cell = itemCellAt(x, y) ?: return
    if (allowsSelection) selection.userSelected(cell)
  }

  /*
   * Show a row's context menu. Returns whether the delegate gave one.
   */
  internal fun showMenu(cell: ShadowListKitListCell): Boolean {
    val delegate = delegate ?: return false
    if (cell.index < 0 || swipe.isOpen || swipe.closingTouch || editingState) return false
    val popup = PopupMenu(context, cell)
    if (!delegate.contextMenuForItem(this, cell.index, popup.menu) || popup.menu.size() == 0) return false
    cell.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    popup.show()
    return true
  }

  internal val hasMenus: Boolean get() = delegate != null

  // endregion

  // region Selection

  val selectedIndices: IntArray get() = selection.selectedIndices

  /*
   * Select an item without delegate calls.
   */
  fun selectItem(index: Int, animated: Boolean = false) = selection.selectItem(index, animated)

  fun deselectItem(index: Int, animated: Boolean = false) = selection.deselectItem(index, animated)

  // endregion

  // region Scroll events

  override fun dispatchTouchEvent(event: MotionEvent): Boolean {
    if (sectionIndex.handle(event)) return true
    if (swipe.handle(event)) return true
    if (drag.handle(event)) return true
    gesture.observe(event)
    return super.dispatchTouchEvent(event)
  }

  /*
   * A drag took over the touch. Rows that saw its start get a cancel.
   */
  internal fun cancelChildTouches() {
    val now = SystemClock.uptimeMillis()
    val cancel = MotionEvent.obtain(now, now, MotionEvent.ACTION_CANCEL, 0f, 0f, 0)
    super.dispatchTouchEvent(cancel)
    cancel.recycle()
    parent?.requestDisallowInterceptTouchEvent(true)
  }

  /*
   * A swipe or a menu took over the touch. The scroll gesture, the hold and the highlight let go.
   */
  internal fun abandonScrollGesture() {
    gesture.abandon()
    drag.cancelPress()
    selection.cancelHighlight()
  }

  /*
   * A touch that closed an open row never reaches the rows. The list keeps it to scroll.
   */
  override fun onInterceptTouchEvent(event: MotionEvent): Boolean =
    gesture.intercept(event) || swipe.closingTouch

  override fun onTouchEvent(event: MotionEvent): Boolean = gesture.touch(event)

  override fun computeScroll() = gesture.computeScroll()

  /*
   * A fling or an animated scroll ended. The core needs an idle frame to let a held
   * correction go, and an animated command lands on its row.
   */
  internal fun scrollingEnded() {
    core.land()
    invalidateFrame()
    delegate?.didEndScrolling(this)
  }

  internal fun gestureBegan() = core.cancelLanding()

  /*
   * A finger started moving the list. The highlight goes and an open row closes.
   */
  internal fun scrollTrackingStarted() {
    selection.cancelHighlight()
    drag.cancelPress()
    if (swipe.isOpen) swipe.close(true)
  }

  /*
   * Where a fling that would stop at target rests instead.
   */
  internal fun snapTarget(target: Int): Int =
    if (snapToItem) core.nearestSnapOffset(target.toDouble()).roundToInt() else target

  /*
   * Whether a drag past the start pulls the refresh spinner.
   */
  internal val canPullToRefresh: Boolean get() = refreshEnabled && !refresh.refreshing && offset <= 0

  internal fun refreshReleased() {
    if (refresh.release()) delegate?.didBeginRefreshing(this)
  }

  override fun computeVerticalScrollRange(): Int =
    if (horizontal) height else max(contentAlong + paddingTop + paddingBottom, height)
  override fun computeVerticalScrollOffset(): Int = scrollY
  override fun computeVerticalScrollExtent(): Int = height
  override fun computeHorizontalScrollRange(): Int =
    if (horizontal) max(contentAlong + paddingLeft + paddingRight, width) else width
  override fun computeHorizontalScrollOffset(): Int = scrollX
  override fun computeHorizontalScrollExtent(): Int = width

  // endregion

  // region Edges

  override fun draw(canvas: Canvas) {
    super.draw(canvas)
    if (gesture.drawEdges(canvas)) postInvalidateOnAnimation()
  }

  // endregion

  // region Nested scrolling

  override fun setNestedScrollingEnabled(enabled: Boolean) {
    nested.isNestedScrollingEnabled = enabled
  }

  override fun isNestedScrollingEnabled(): Boolean = nested.isNestedScrollingEnabled
  override fun startNestedScroll(axes: Int): Boolean = nested.startNestedScroll(axes)
  override fun startNestedScroll(axes: Int, type: Int): Boolean = nested.startNestedScroll(axes, type)
  override fun stopNestedScroll() = nested.stopNestedScroll()
  override fun stopNestedScroll(type: Int) = nested.stopNestedScroll(type)
  override fun hasNestedScrollingParent(): Boolean = nested.hasNestedScrollingParent()
  override fun hasNestedScrollingParent(type: Int): Boolean = nested.hasNestedScrollingParent(type)

  override fun dispatchNestedScroll(
    dxConsumed: Int, dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int, offsetInWindow: IntArray?,
  ): Boolean = nested.dispatchNestedScroll(dxConsumed, dyConsumed, dxUnconsumed, dyUnconsumed, offsetInWindow)

  override fun dispatchNestedScroll(
    dxConsumed: Int, dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int, offsetInWindow: IntArray?, type: Int,
  ): Boolean = nested.dispatchNestedScroll(dxConsumed, dyConsumed, dxUnconsumed, dyUnconsumed, offsetInWindow, type)

  override fun dispatchNestedScroll(
    dxConsumed: Int, dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int, offsetInWindow: IntArray?, type: Int,
    consumed: IntArray,
  ) = nested.dispatchNestedScroll(dxConsumed, dyConsumed, dxUnconsumed, dyUnconsumed, offsetInWindow, type, consumed)

  override fun dispatchNestedPreScroll(dx: Int, dy: Int, consumed: IntArray?, offsetInWindow: IntArray?): Boolean =
    nested.dispatchNestedPreScroll(dx, dy, consumed, offsetInWindow)

  override fun dispatchNestedPreScroll(dx: Int, dy: Int, consumed: IntArray?, offsetInWindow: IntArray?, type: Int): Boolean =
    nested.dispatchNestedPreScroll(dx, dy, consumed, offsetInWindow, type)

  override fun dispatchNestedFling(velocityX: Float, velocityY: Float, consumed: Boolean): Boolean =
    nested.dispatchNestedFling(velocityX, velocityY, consumed)

  override fun dispatchNestedPreFling(velocityX: Float, velocityY: Float): Boolean =
    nested.dispatchNestedPreFling(velocityX, velocityY)

  // endregion

  // region Queries

  fun cellForItem(index: Int): ShadowListKitListCell? {
    return mountedCell(rowForItem(index))?.takeIf { it.visibility == VISIBLE }
  }

  val visibleCells: List<ShadowListKitListCell>
    get() {
      val low = offset + leadingPadding
      val high = low + windowAlong
      return mounted.values
        .filter { it.visibility == VISIBLE && it.index >= 0 && (if (horizontal) it.right > low && it.left < high else it.bottom > low && it.top < high) }
        .sortedBy { it.row }
    }

  /*
   * Size of the content along the scroll axis, header and footer included, in pixels.
   */
  val contentSize: Int get() = contentAlong

  /*
   * The items among the rows overlapping the viewport, low to high, or null before the first layout.
   */
  val visibleRange: IntRange?
    get() {
      // A list without a core has not laid out. Asking would create a peer only to answer null.
      val packed = liveCore?.visibleItemRange() ?: return null
      if (packed < 0) return null
      return (packed shr 32).toInt()..(packed and 0xffffffffL).toInt()
    }

  /*
   * The item's frame in the list's scrolled coordinates, the same space as the cells' frames,
   * padding included. Estimated when the row was not measured yet.
   */
  fun rectForItem(index: Int): RectF? = rowRectF(rowForItem(index))

  private fun rowRectF(row: Int): RectF? {
    if (row < 0) return null
    val out = DoubleArray(4)
    if (!core.rowRect(row, out)) return null
    val left = (out[0] + paddingLeft).toFloat()
    val top = (out[1] + paddingTop).toFloat()
    return RectF(left, top, left + out[2].toFloat(), top + out[3].toFloat())
  }

  // endregion

  // region Saved position

  /*
   * The row at the viewport start by key and how far the viewport is into it, or null before
   * the first layout.
   */
  val anchorState: ShadowListKitAnchorState?
    get() {
      val current = coreOrNull ?: return pendingAnchor
      val out = DoubleArray(1)
      val key = current.anchor(offset.toDouble(), out) ?: return pendingAnchor
      return ShadowListKitAnchorState(key, out[0].toFloat())
    }

  /*
   * Land the saved row the same distance in again, now or once a reload brings its key.
   */
  fun restoreAnchorState(state: ShadowListKitAnchorState) {
    pendingAnchor = state
    restorePendingAnchor()
  }

  private fun restorePendingAnchor() {
    val state = pendingAnchor ?: return
    if (keys.isEmpty() || !core.restoreAnchor(state.key, state.offset.toDouble())) return
    pendingAnchor = null
    stopScrolling()
    runCommandNow()
  }

  override fun onSaveInstanceState(): Parcelable? {
    val superState = super.onSaveInstanceState()
    val anchor = anchorState ?: return superState
    return Bundle().apply {
      putParcelable(STATE_SUPER, superState)
      putParcelable(STATE_ANCHOR, anchor)
    }
  }

  @Suppress("DEPRECATION")
  override fun onRestoreInstanceState(state: Parcelable?) {
    if (state !is Bundle) {
      super.onRestoreInstanceState(state)
      return
    }
    state.classLoader = ShadowListKitAnchorState::class.java.classLoader
    super.onRestoreInstanceState(state.getParcelable(STATE_SUPER))
    state.getParcelable<ShadowListKitAnchorState>(STATE_ANCHOR)?.let(::restoreAnchorState)
  }

  // endregion

  // region Accessibility

  override fun getAccessibilityClassName(): CharSequence =
    if (numberOfColumns > 1) "android.widget.GridView" else "android.widget.ListView"

  /*
   * The list tells accessibility services how many rows it holds, not only the mounted ones,
   * and offers page scrolls and scrolling to any row. TalkBack scrolls forward when focus
   * leaves the last mounted row.
   */
  override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
    super.onInitializeAccessibilityNodeInfo(info)
    info.isScrollable = maxOffset > 0
    if (offset > 0) {
      info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_BACKWARD)
      info.addAction(if (horizontal) AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_LEFT
        else AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP)
    }
    if (offset < maxOffset) {
      info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD)
      info.addAction(if (horizontal) AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_RIGHT
        else AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN)
    }
    info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_TO_POSITION)
    val lines = (itemCount + numberOfColumns - 1) / numberOfColumns
    val mode = if (allowsMultipleSelection) AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_MULTIPLE
      else if (allowsSelection) AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_SINGLE
      else AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_NONE
    info.collectionInfo = if (horizontal) {
      AccessibilityNodeInfo.CollectionInfo.obtain(numberOfColumns, lines, false, mode)
    } else {
      AccessibilityNodeInfo.CollectionInfo.obtain(lines, numberOfColumns, false, mode)
    }
  }

  override fun onInitializeAccessibilityEvent(event: AccessibilityEvent) {
    super.onInitializeAccessibilityEvent(event)
    event.isScrollable = maxOffset > 0
    event.itemCount = itemCount
    visibleRange?.let {
      event.fromIndex = it.first
      event.toIndex = it.last
    }
    if (horizontal) {
      event.scrollX = offset
      event.maxScrollX = maxOffset
    } else {
      event.scrollY = offset
      event.maxScrollY = maxOffset
    }
  }

  override fun performAccessibilityAction(action: Int, arguments: Bundle?): Boolean {
    val forward = if (horizontal) android.R.id.accessibilityActionScrollRight else android.R.id.accessibilityActionScrollDown
    val backward = if (horizontal) android.R.id.accessibilityActionScrollLeft else android.R.id.accessibilityActionScrollUp
    return when (action) {
      AccessibilityNodeInfo.ACTION_SCROLL_FORWARD, forward -> scrollByPage(1)
      AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD, backward -> scrollByPage(-1)
      android.R.id.accessibilityActionScrollToPosition -> scrollToPosition(arguments)
      else -> super.performAccessibilityAction(action, arguments)
    }
  }

  /*
   * One viewport toward the end, or toward the start for a negative direction.
   */
  private fun scrollByPage(direction: Int): Boolean {
    val target = ShadowListKitCore.pageScrollTarget(offset.toDouble(), windowAlong.toDouble(), maxOffset.toDouble(), direction).roundToInt()
    if (target == offset) return false
    gesture.stop()
    writeOffset(target, byUser = true)
    layoutPass()
    sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED)
    return true
  }

  private fun scrollToPosition(arguments: Bundle?): Boolean {
    val row = arguments?.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_ROW_INT, -1) ?: -1
    val column = arguments?.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_COLUMN_INT, 0) ?: 0
    val index = if (horizontal) column * numberOfColumns + row else row * numberOfColumns + column
    if (index !in 0 until itemCount) return false
    scrollToItem(index)
    sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED)
    return true
  }

  // endregion

  // region Scroll commands

  /*
   * Bring an item into view. viewPosition is where it rests in the viewport, from 0 at the start
   * to 1 at the end. The core keeps correcting until the row lands, even across estimates.
   */
  fun scrollToItem(index: Int, viewPosition: Double = 0.0, animated: Boolean = false) {
    val row = rowForItem(index)
    if (row >= 0) scrollToRow(row, viewPosition, animated)
  }

  private fun scrollToRow(row: Int, viewPosition: Double, animated: Boolean) {
    if (row !in keys.indices) return
    val target = if (animated) core.animatedTargetOffset(row, viewPosition, windowAlong.toDouble(), maxOffset.toDouble())
      else Double.NaN
    if (!target.isNaN()) {
      // Animate to the estimate, then let the core land exactly when the animation ends.
      animateCommand(target.roundToInt(), ShadowListKitCore.LANDING_INDEX, row, viewPosition)
      return
    }
    stopScrolling()
    core.scrollToIndex(row, viewPosition)
    runCommandNow()
  }

  fun scrollToStart(animated: Boolean = false) {
    if (animated) {
      // The header shows at the very start, which landing on row 0 would scroll past.
      animateCommand(0, ShadowListKitCore.LANDING_START)
      return
    }
    stopScrolling()
    core.scrollToStart()
    runCommandNow()
  }

  fun scrollToEnd(animated: Boolean = false) {
    if (animated) {
      animateCommand(maxOffset, ShadowListKitCore.LANDING_END)
      return
    }
    stopScrolling()
    core.scrollToEnd()
    runCommandNow()
  }

  fun closeSwipeActions(animated: Boolean = true) = swipe.close(animated)

  private fun animateCommand(target: Int, landing: Int, index: Int = 0, viewPosition: Double = 0.0) {
    core.setLanding(landing, index, viewPosition)
    gesture.animateTo(target)
  }

  /*
   * Stop momentum and any animated command still on its way. Otherwise they keep writing
   * the offset over a command that runs now.
   */
  private fun stopScrolling() {
    gesture.stop()
    core.cancelLanding()
  }

  internal fun runCommandNow() {
    needsFrame = true
    if (isLaidOut && !isLayoutRequested) layoutPass() else requestLayout()
  }

  // endregion

  // region Drag

  /*
   * A hold on any row lifts it, the one swiped open too. The open row closes, the same as on iOS.
   */
  internal fun beginDragAt(x: Float, y: Float): Boolean =
    reorderEnabled && !editingState && drag.begin(x, y)

  internal val hasHeldRow: Boolean get() = drag.hasHeldRow

  internal fun mountedIndices(): IntArray {
    val indices = IntArray(mounted.size)
    var count = 0
    for (cell in mounted.values) {
      if (cell.visibility == VISIBLE && cell.row >= 0) indices[count++] = cell.row
    }
    return indices.copyOf(count)
  }

  /*
   * The held row was dropped. Move it in the data and keep it where it was let go. Returns
   * whether an item moved. In a list with sections it stays in its section.
   */
  internal fun commitMove(fromRow: Int, toRow: Int): Boolean {
    val from = itemForRow(fromRow)
    val to = if (isSectioned) core.itemForDrop(fromRow, toRow) else toRow
    if (from < 0 || to < 0 || from == to) return false
    delegate?.moveItem(this, from, to)
    // The drop animates the rows itself.
    val animates = animatesChanges
    animatesChanges = false
    reloadData()
    animatesChanges = animates
    layoutPass()
    return true
  }

  // endregion

  private companion object {
    const val STATE_SUPER = "ShadowListKitListView.super"
    const val STATE_ANCHOR = "ShadowListKitListView.anchor"
  }
}

/*
 * The cell of a section header or footer the data source gives only a title for.
 */
internal class ShadowListKitSectionTitleCell(context: Context, identifier: String) : ShadowListKitListCell(context, identifier) {
  companion object {
    const val HEADER = "ShadowListKitSectionHeader"
    const val FOOTER = "ShadowListKitSectionFooter"
    private const val INSET_HORIZONTAL_DP = 16
    private const val INSET_VERTICAL_DP = 6
    private const val MIN_HEIGHT_DP = 28
  }

  private val label = TextView(context)

  var title: CharSequence
    get() = label.text
    set(value) { label.text = value }

  init {
    val density = resources.displayMetrics.density
    val night = (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
    val footer = identifier == FOOTER
    label.setTextColor(if (night) 0xFF98989F.toInt() else 0xFF6D6D72.toInt())
    label.textSize = if (footer) 13f else 15f
    if (!footer) label.setTypeface(label.typeface, Typeface.BOLD)
    val horizontalInset = (INSET_HORIZONTAL_DP * density).roundToInt()
    val verticalInset = (INSET_VERTICAL_DP * density).roundToInt()
    label.setPadding(horizontalInset, verticalInset, horizontalInset, verticalInset)
    label.minHeight = (MIN_HEIGHT_DP * density).roundToInt()
    label.gravity = Gravity.CENTER_VERTICAL
    if (!footer) setBackgroundColor(if (night) 0xFF1C1C1E.toInt() else 0xFFF2F2F7.toInt())
    addView(label, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
  }
}
