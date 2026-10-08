package com.shadowlist.kit

import android.content.Context
import android.content.res.Configuration
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Bundle
import android.os.Parcelable
import android.util.AttributeSet
import android.util.Log
import android.view.HapticFeedbackConstants
import android.view.Menu
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
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
open class SLKListView @JvmOverloads constructor(
  context: Context,
  attrs: AttributeSet? = null,
) : ViewGroup(context, attrs), NestedScrollingChild3 {

  interface DataSource {
    fun numberOfItems(listView: SLKListView): Int

    /*
     * A stable identity for the row. Sizes, cells and the scroll position follow keys across
     * reloadData. A prepend or an insert above keeps what is on screen in place.
     * A row whose content changed under the same key needs reloadItems.
     */
    fun keyForItem(listView: SLKListView, index: Int): String

    fun cellForItem(listView: SLKListView, index: Int): SLKListCell

    /*
     * Update a shown cell for a payload given to reloadItems without a new cell. Return false
     * to have the row reloaded in full instead.
     */
    fun reconfigureCell(listView: SLKListView, cell: SLKListCell, index: Int, payload: Any?): Boolean = false
  }

  /*
   * Implemented by a data source that knows row sizes without a view, like from a layout
   * precomputed off the UI thread. Otherwise every row is measured once through its cell.
   */
  interface Sizing {
    fun sizeForItem(listView: SLKListView, index: Int, crossSize: Int): Int
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
    fun numberOfSections(listView: SLKListView): Int
    fun numberOfItemsInSection(listView: SLKListView, section: Int): Int
    fun keyForSection(listView: SLKListView, section: Int): String? = null
    fun titleForHeaderInSection(listView: SLKListView, section: Int): String? = null
    fun titleForFooterInSection(listView: SLKListView, section: Int): String? = null
    fun cellForHeaderInSection(listView: SLKListView, section: Int): SLKListCell? = null
    fun cellForFooterInSection(listView: SLKListView, section: Int): SLKListCell? = null
    fun sizeForHeaderInSection(listView: SLKListView, section: Int, crossSize: Int): Int = -1
    fun sizeForFooterInSection(listView: SLKListView, section: Int, crossSize: Int): Int = -1
    fun sectionIndexTitles(listView: SLKListView): List<String>? = null
    fun sectionForSectionIndexTitle(listView: SLKListView, title: String, index: Int): Int = index
  }

  /*
   * Implemented by a data source whose items change content under the same key. The value
   * changes with the content, like a revision or a hash. applyChanges reloads the rows whose
   * value changed.
   */
  interface ContentVersions {
    fun contentVersionForItem(listView: SLKListView, index: Int): Long
  }

  /*
   * Hears which items the list will soon show, to load what their cells need ahead. Prefetched
   * items are the ones in the core's measured window, an overscan past the viewport, that have
   * no cell yet. A prefetched item that leaves the window before it shows is cancelled.
   */
  interface PrefetchDataSource {
    fun prefetchItems(listView: SLKListView, indices: IntArray)
    fun cancelPrefetchingForItems(listView: SLKListView, indices: IntArray) {}
  }

  interface Delegate {
    fun willDisplayCell(listView: SLKListView, cell: SLKListCell, index: Int) {}
    fun didEndDisplayingCell(listView: SLKListView, cell: SLKListCell, index: Int) {}
    fun didSelectItem(listView: SLKListView, index: Int) {}
    fun didDeselectItem(listView: SLKListView, index: Int) {}
    fun shouldSelectItem(listView: SLKListView, index: Int): Boolean = true
    fun shouldHighlightItem(listView: SLKListView, index: Int): Boolean = true
    fun didScroll(listView: SLKListView) {}

    /*
     * The list came to rest after a touch, a fling or an animated scroll, on a row edge when
     * snapToItem is set.
     */
    fun didEndScrolling(listView: SLKListView) {}

    /*
     * Whether a row can be picked up when reorderEnabled is set. Every row can by default.
     */
    fun canMoveItem(listView: SLKListView, index: Int): Boolean = true

    /*
     * A held row was dropped at another index. Move the item in the data, the list reads the
     * data again right after and keeps the dropped row where it was let go.
     */
    fun moveItem(listView: SLKListView, sourceIndex: Int, destinationIndex: Int) {}

    /*
     * The scroll position came within startReachedThreshold or endReachedThreshold of an edge.
     */
    fun didReachStart(listView: SLKListView) {}
    fun didReachEnd(listView: SLKListView) {}

    /*
     * Actions behind a row swiped from its leading or trailing side, or null for none.
     */
    fun leadingSwipeActionsForItem(listView: SLKListView, index: Int): SLKSwipeActionsConfiguration? = null
    fun trailingSwipeActionsForItem(listView: SLKListView, index: Int): SLKSwipeActionsConfiguration? = null

    /*
     * Fill the menu for touching and holding a row and return true, or false for none. A row
     * that can also be reordered lifts on the hold. Letting go without moving it shows the menu.
     */
    fun contextMenuForItem(listView: SLKListView, index: Int, menu: Menu): Boolean = false

    /*
     * Whether the separator below an item shows when showsSeparators is set. Separators only
     * go between items of one section.
     */
    fun showsSeparatorAfterItem(listView: SLKListView, index: Int): Boolean = true

    /*
     * The reader pulled to refresh with refreshEnabled. Set refreshing to false when done.
     */
    fun didBeginRefreshing(listView: SLKListView) {}
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

  // Size along the scroll axis assumed for rows not measured yet, in pixels.
  var estimatedItemSize = 120 * resources.displayMetrics.density
    set(value) { field = value; sendSettings() }

  // How far past the viewport rows are measured, in viewport sizes. Default 1.
  var overscan = 1.0
    set(value) { field = value; sendSettings() }

  // How far past the viewport cells are mounted, in viewport sizes. Default 0.5.
  var mountOverscan = 0.5
    set(value) { field = value; invalidateFrame() }

  // Distances to an edge, in viewport sizes, that fire the reached callbacks. Default 1.
  var startReachedThreshold = 1.0
    set(value) { field = value; sendSettings() }
  var endReachedThreshold = 1.0
    set(value) { field = value; sendSettings() }

  // Rest the scroll position on a row edge. Alignment 0 start, 1 center, 2 end.
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

  var itemAnimator: SLKItemAnimator = SLKDefaultItemAnimator()

  // Items that stick to the top of the viewport once scrolled past.
  var stickyIndices: IntArray = IntArray(0)
    set(value) {
      field = value.sortedArray()
      updateStickyRows()
      invalidateFrame()
    }

  // Every section header sticks to the top of the viewport once scrolled past.
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
      if (!value) clearSelection()
    }
  var allowsMultipleSelection = false
    set(value) {
      field = value
      if (!value && selectedKeys.size > 1) clearSelection()
    }

  // Passed to the cells. Turns swipe actions and reordering off.
  var editing: Boolean
    get() = editingState
    set(value) = setEditing(value, false)

  // Pull to refresh with a spinner the list draws. refreshing shows it spinning.
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
   * new core with the same settings, keys and sections. Rows are measured again.
   */
  private var coreOrNull: SLKCore? = null
  internal val core: SLKCore
    get() = coreOrNull ?: createCore()

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

  private val changes = SLKChangeAnimator(this)
  private val gesture = SLKScrollGesture(this)
  private val drag = SLKDragController(this)
  private val swipe = SLKSwipeController(this)
  internal val refresh = SLKRefreshIndicator(this)
  private val sectionIndex = SLKSectionIndex(this)

  // Row keys in data order, the same list the core holds. Section headers and footers included.
  private var keys = ArrayList<String>()

  // The list reloadData reads the next keys into, swapped with keys after.
  private var spareKeys = ArrayList<String>()

  /*
   * The sections, null without. rowItems has the item of every row, -1 for headers and footers,
   * and rowSeparators whether a separator follows the row. A list without sections maps rows to
   * the same items and makes no call into the core for it.
   */
  private var sectionCounts: IntArray? = null
  private var sectionFlags: IntArray? = null
  private var rowItems: IntArray? = null
  private var rowSeparators: BooleanArray? = null
  private var itemCount = 0

  // Mounted cells by key. A cell follows its key across inserts above it.
  internal val mounted = HashMap<String, SLKListCell>()
  private var mountGeneration = 0L

  // What the last mount pass covered. The pass is skipped while all of it holds.
  private var mountedLow = -1
  private var mountedHigh = -1
  private var mountedSticky = -1
  private var mountedGeometry = -1.0
  private var mountedStructure = 0
  private var structureVersion = 0

  private val cellFactories = HashMap<String, (Context) -> SLKListCell>()
  private val reusePool = HashMap<String, ArrayList<SLKListCell>>()

  // The rows the core keeps measured and their frames, four values each.
  private var windowLow = -1
  private var windowHigh = -1
  private var windowFrames = DoubleArray(256)
  private var windowGeometry = -1.0
  private val scratchFrame = DoubleArray(4)

  private var stickyCell: SLKListCell? = null

  // The sticky rows: the sticky items' rows and the section headers, sorted.
  private var stickyRows = IntArray(0)

  /*
   * Leading edge and extent of every sticky row, copied from the core when the geometry
   * changes. Pinning reads only these on a scroll frame.
   */
  private var stickyFrames = DoubleArray(0)
  private var stickyGeometry = -1.0
  private var frameGeometry = 0.0

  // Child positions of the views drawn last, see dispatchDraw.
  private var liftedLow = -1
  private var liftedHigh = -1
  private var liftedTop = -1

  // The band of offsets where the core has nothing to do.
  private var bandLow = 1.0
  private var bandHigh = 0.0
  private var needsFrame = true

  // Geometry the core last ran with.
  internal var windowAlong = 0
    private set
  internal var windowCross = 0
    private set
  private var headerSize = 0
  private var footerSize = 0
  internal var contentAlong = 0
    private set

  // The last offset seen, to tell the user's scrolling from our own writes.
  private var previousOffset = 0
  private var userScrolled = false

  private var inLayoutPass = false
  private var settleScheduled = false
  private var reachedStart = false
  private var reachedEnd = false

  // Changes collected by performBatchUpdates until its block returns.
  private var batchDepth = 0
  private val batchDeleted = ArrayList<Int>()
  private val batchInserted = ArrayList<Int>()
  private val batchMovedFrom = ArrayList<Int>()
  private val batchMovedTo = ArrayList<Int>()
  private val batchReloaded = ArrayList<Int>()
  private var batchPayload: Any? = null
  private var batchNeedsReload = false

  /*
   * Selected rows by key. The same rules as the core's ListSelection the UIKit list uses, kept
   * here because the selection outlives the core, which is dropped on detach.
   */
  private val selectedKeys = LinkedHashSet<String>()
  private var editingState = false
  private var highlightedCell: SLKListCell? = null
  private val highlightRunnable = Runnable { highlightPending() }
  private var highlightX = 0f
  private var highlightY = 0f

  // The content version of every item applyChanges saw, by key.
  private var contentVersions = HashMap<String, Long>()

  private var pendingAnchor: SLKAnchorState? = null
  private val decorations = ArrayList<SLKItemDecoration>()
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
    drag.end(false)
    swipe.close(false)
    destroyCore()
  }

  // region Axis

  internal fun along(x: Float, y: Float): Float = if (horizontal) x else y
  internal fun cross(x: Float, y: Float): Float = if (horizontal) y else x

  // The padding before the rows along and across the scroll axis. Rows start there.
  internal val leadingPadding: Int get() = if (horizontal) paddingLeft else paddingTop
  internal val crossPadding: Int get() = if (horizontal) paddingTop else paddingLeft
  private val trailingPadding: Int get() = if (horizontal) paddingRight else paddingBottom

  // The scroll offset along the axis, in pixels.
  internal val offset: Int get() = if (horizontal) scrollX else scrollY

  internal val maxOffset: Int get() = max(0, contentAlong - windowAlong)

  /*
   * A point in the list's own coordinates as a position in the content the core places rows
   * in, along and across the scroll axis.
   */
  internal fun contentAlongAt(x: Float, y: Float): Float = along(x, y) + offset - leadingPadding
  internal fun contentCrossAt(x: Float, y: Float): Float = cross(x, y) - crossPadding

  // A point in the list's own coordinates as a position in the padded window along the axis.
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
    gesture.isTracking -> SLKCore.PHASE_DRAGGING
    gesture.isFlinging -> SLKCore.PHASE_SETTLING
    else -> SLKCore.PHASE_IDLE
  }

  // endregion

  // region Properties

  private fun sendSettings() {
    coreOrNull?.let(::applySettings)
    invalidateFrame()
  }

  private fun applySettings(target: SLKCore) {
    target.setSettings(estimatedItemSize.toDouble(), overscan, startReachedThreshold, endReachedThreshold,
      numberOfColumns, inverted, followAppends, horizontal, snapToItem, snapAlignment)
  }

  /*
   * A new core with this list's settings, sticky rows, sections and keys. Every copy of its
   * output here is dropped and the next layout pass runs it.
   */
  private fun createCore(): SLKCore {
    val created = SLKCore(::measureItem)
    coreOrNull = created
    applySettings(created)
    if (stickyRows.isNotEmpty()) created.setStickyIndices(stickyRows)
    sectionCounts?.let { created.setSections(0, it, sectionFlags) }
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

  fun addItemDecoration(decoration: SLKItemDecoration) {
    decorations.add(decoration)
    invalidate()
  }

  fun removeItemDecoration(decoration: SLKItemDecoration) {
    decorations.remove(decoration)
    invalidate()
  }

  // endregion

  // region Cells

  fun registerCell(identifier: String, factory: (Context) -> SLKListCell) {
    cellFactories[identifier] = factory
  }

  @Suppress("UNCHECKED_CAST")
  fun <T : SLKListCell> dequeueReusableCell(identifier: String): T {
    val pool = reusePool[identifier]
    if (pool != null && pool.isNotEmpty()) {
      val cell = pool.removeAt(pool.size - 1)
      cell.prepareForReuse()
      return cell as T
    }
    val factory = cellFactories[identifier] ?: { context: Context -> SLKListCell(context, identifier) }
    val cell = factory(context)
    cell.visibility = INVISIBLE
    addViewInLayout(cell, -1, generateDefaultLayoutParams(), true)
    return cell as T
  }

  internal fun recycleCell(cell: SLKListCell) {
    val index = cell.index
    swipe.cellWillRecycle(cell)
    cell.animate().cancel()
    cell.alpha = 1f
    cell.translationX = 0f
    cell.translationY = 0f
    cell.visibility = INVISIBLE
    cell.index = SLKListCell.NO_INDEX
    cell.row = SLKListCell.NO_INDEX
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
  private fun makeCell(row: Int): SLKListCell {
    val item = itemForRow(row)
    val cell = if (item >= 0) requireNotNull(dataSource).cellForItem(this, item) else sectionCell(row)
    if (cell.parent !== this) {
      addViewInLayout(cell, -1, generateDefaultLayoutParams(), true)
    }
    cell.row = row
    cell.index = item
    return cell
  }

  private fun sectionCell(row: Int): SLKListCell {
    val place = core.placeOfRow(row)
    val section = place shr 2
    val footer = (place and 3) == SLKCore.ROW_FOOTER
    val sections = dataSource as Sections
    val custom = if (footer) sections.cellForFooterInSection(this, section) else sections.cellForHeaderInSection(this, section)
    if (custom != null) return custom
    val identifier = if (footer) SLKSectionTitleCell.FOOTER else SLKSectionTitleCell.HEADER
    if (identifier !in cellFactories) registerCell(identifier) { SLKSectionTitleCell(it, identifier) }
    val cell = dequeueReusableCell<SLKSectionTitleCell>(identifier)
    cell.title = (if (footer) sections.titleForFooterInSection(this, section) else sections.titleForHeaderInSection(this, section)) ?: ""
    return cell
  }

  internal fun keyAt(index: Int): String? = keys.getOrNull(index)

  // The mounted cell of a row, or null.
  private fun mountedCell(row: Int): SLKListCell? = keyAt(row)?.let { mounted[it] }

  override fun generateDefaultLayoutParams(): LayoutParams =
    LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT)

  // endregion

  // region Data

  /*
   * Read the sections and every key into next. A list with sections gets its header and footer
   * rows there.
   */
  private fun readRowKeys(next: ArrayList<String>) {
    val source = dataSource ?: return
    next.clear()
    val sections = source as? Sections
    if (sections == null) {
      val count = max(0, source.numberOfItems(this))
      next.ensureCapacity(count)
      for (index in 0 until count) next.add(source.keyForItem(this, index))
      setPlainSections(count)
      return
    }
    val sectionCount = max(0, sections.numberOfSections(this))
    val counts = IntArray(sectionCount)
    val flags = IntArray(sectionCount)
    var item = 0
    for (section in 0 until sectionCount) {
      val count = max(0, sections.numberOfItemsInSection(this, section))
      val hasHeader = sections.titleForHeaderInSection(this, section) != null
      val hasFooter = sections.titleForFooterInSection(this, section) != null
      counts[section] = count
      flags[section] = (if (hasHeader) SLKCore.SECTION_HEADER else 0) or (if (hasFooter) SLKCore.SECTION_FOOTER else 0)
      val headerAt = next.size
      if (hasHeader) next.add("")
      for (local in 0 until count) next.add(source.keyForItem(this, item + local))
      val sectionKey = sections.keyForSection(this, section)
        ?: if (count > 0) next[headerAt + if (hasHeader) 1 else 0] else "#$section"
      if (hasHeader) next[headerAt] = SLKCore.HEADER_KEY_PREFIX + sectionKey
      if (hasFooter) next.add(SLKCore.FOOTER_KEY_PREFIX + sectionKey)
      item += count
    }
    setSections(counts, flags, next.size)
  }

  private fun setPlainSections(count: Int) {
    sectionCounts = null
    sectionFlags = null
    rowItems = null
    rowSeparators = null
    itemCount = count
  }

  private fun setSections(counts: IntArray, flags: IntArray, rows: Int) {
    sectionCounts = counts
    sectionFlags = flags
    itemCount = counts.sum()
    coreOrNull?.setSections(0, counts, flags)
    val items = IntArray(rows)
    val separators = BooleanArray(rows)
    core.copyRows(items, separators)
    rowItems = items
    rowSeparators = separators
  }

  internal val isSectioned: Boolean get() = sectionCounts != null

  /*
   * Read the row count and keys again. Rows keep their sizes and cells by key, and the
   * visible content stays in place. Cells of surviving keys are not configured again.
   * Only the keys between the unchanged rows at both ends go to the core.
   */
  fun reloadData() {
    if (batchDepth > 0) {
      batchNeedsReload = true
      return
    }
    if (dataSource == null) return
    val next = spareKeys
    readRowKeys(next)
    recordContentVersions(next)
    applyRowKeys(next)
    reloadSectionIndex()
  }

  /*
   * The content version of every item now, which the next applyChanges compares against.
   */
  private fun recordContentVersions(rows: List<String>) {
    val versioned = dataSource as? ContentVersions ?: return
    val items = itemKeysOf(rows, rowItems)
    val versions = HashMap<String, Long>(items.size * 2)
    for ((item, key) in items.withIndex()) versions[key] = versioned.contentVersionForItem(this, item)
    contentVersions = versions
  }

  private fun applyRowKeys(next: ArrayList<String>) {
    val start = commonPrefix(keys, next)
    val end = commonSuffix(keys, next, start)
    if (start < keys.size || start < next.size) {
      if (animatesChanges) changes.capture(removed = keys.subList(start, keys.size - end), inserted = next.subList(start, next.size - end))
      coreOrNull?.replaceKeys(start, keys.size - start - end, next, start, next.size - end)
    }
    spareKeys = keys
    keys = next
    structureChanged()
  }

  /*
   * Rows were inserted at these positions of the new data, which the data source already
   * reflects. Only the new keys are read. A list with sections reads everything again.
   */
  fun insertItems(indices: IntArray) {
    if (batchDepth > 0) {
      batchInserted.addAll(indices.asList())
      return
    }
    if (dataSource is Sections) {
      reloadData()
      return
    }
    val source = dataSource ?: return
    val sorted = validIndices(indices, keys.size + indices.size)
    if (sorted.isEmpty()) return
    val inserted = Array(sorted.size) { source.keyForItem(this, sorted[it]) }
    if (animatesChanges) changes.capture(removed = emptyList(), inserted = inserted.asList())
    forEachRun(sorted) { first, last ->
      keys.addAll(min(sorted[first], keys.size), inserted.asList().subList(first, last + 1))
    }
    coreOrNull?.insertKeys(sorted, inserted)
    itemCount = keys.size
    structureChanged()
  }

  /*
   * Rows were deleted at these positions of the old data.
   */
  fun deleteItems(indices: IntArray) {
    if (batchDepth > 0) {
      batchDeleted.addAll(indices.asList())
      return
    }
    if (dataSource is Sections) {
      reloadData()
      return
    }
    val sorted = validIndices(indices, keys.size)
    if (sorted.isEmpty()) return
    if (animatesChanges) changes.capture(removed = sorted.map { keys[it] }, inserted = emptyList())
    // Runs go last to first, which keeps the earlier indices valid.
    val runs = ArrayList<IntArray>()
    forEachRun(sorted) { first, last -> runs.add(intArrayOf(sorted[first], sorted[last])) }
    for (run in runs.asReversed()) keys.subList(run[0], run[1] + 1).clear()
    coreOrNull?.deleteKeys(sorted)
    itemCount = keys.size
    structureChanged()
  }

  /*
   * The rows' content changed under the same keys. Visible cells are configured again and
   * every listed row is measured again. With a payload the data source's reconfigureCell can
   * update a shown cell instead.
   */
  @JvmOverloads
  fun reloadItems(indices: IntArray, payload: Any? = null) {
    if (batchDepth > 0) {
      batchReloaded.addAll(indices.asList())
      batchPayload = payload
      return
    }
    val rows = indices.map(::rowForItem).filter { it >= 0 }.toIntArray()
    reloadRows(rows, payload)
  }

  private fun reloadRows(rows: IntArray, payload: Any?) {
    for (row in rows) {
      val key = keys.getOrNull(row) ?: continue
      val cell = mounted[key] ?: continue
      val item = itemForRow(row)
      if (payload != null && item >= 0 && dataSource?.reconfigureCell(this, cell, item, payload) == true) continue
      mounted.remove(key)
      recycleCell(cell)
    }
    coreOrNull?.markRemeasure(rows)
    structureChanged()
  }

  // An item moved, after the data source reflects it.
  fun moveItem(index: Int, newIndex: Int) {
    if (index < 0 || newIndex < 0) return
    performBatchUpdates({
      batchMovedFrom.add(index)
      batchMovedTo.add(newIndex)
    })
  }

  /*
   * Inserts, deletes, moves and reloads made in updates land together in one layout and one
   * animation, the way UITableView takes them: deletes, reloads and move sources are indices in
   * the data before, inserts and move destinations in the data after. A batch that does not add
   * up reloads everything. completion runs once the change animation ended.
   */
  @JvmOverloads
  fun performBatchUpdates(updates: () -> Unit, completion: ((finished: Boolean) -> Unit)? = null) {
    ++batchDepth
    try {
      updates()
    } finally {
      --batchDepth
    }
    if (batchDepth == 0) commitBatch()
    if (completion == null) return
    val wait = if (animatesChanges && isAttachedToWindow) (itemAnimator as? SLKDefaultItemAnimator)?.durationMs ?: 0L else 0L
    runCommandNow()
    postDelayed({ completion(true) }, wait)
  }

  /*
   * Apply the changes a batch collected in one go. The core's batch plan builds the next keys
   * from the keys held and the new ones it reads. A list with sections, or a batch that does
   * not add up, reads everything again.
   */
  private fun commitBatch() {
    val deleted = batchDeleted.toIntArray()
    val inserted = batchInserted.toIntArray()
    val movedFrom = batchMovedFrom.toIntArray()
    val movedTo = batchMovedTo.toIntArray()
    val reloaded = batchReloaded.toIntArray()
    val payload = batchPayload
    val needsReload = batchNeedsReload
    batchDeleted.clear()
    batchInserted.clear()
    batchMovedFrom.clear()
    batchMovedTo.clear()
    batchReloaded.clear()
    batchPayload = null
    batchNeedsReload = false
    val source = dataSource ?: return
    if (deleted.isEmpty() && inserted.isEmpty() && movedFrom.isEmpty() && reloaded.isEmpty() && !needsReload) return
    // The reloaded rows by key, from the data before.
    val reloadedKeys = HashSet<String>()
    for (item in reloaded) keys.getOrNull(rowForItem(item))?.let(reloadedKeys::add)
    val next = spareKeys
    next.clear()
    var planned = false
    if (source !is Sections && !needsReload) {
      val nextCount = max(0, source.numberOfItems(this))
      val plan = SLKCore.planBatch(keys.size, nextCount, deleted, inserted, movedFrom, movedTo)
      if (plan != null) {
        next.ensureCapacity(plan.size)
        for ((index, from) in plan.withIndex()) next.add(if (from < 0) source.keyForItem(this, index) else keys[from])
        setPlainSections(next.size)
        planned = true
      } else {
        Log.w("SLKListView", "batch updates do not add up to $nextCount items, reloading")
      }
    }
    if (!planned) readRowKeys(next)
    applyRowKeys(next)
    val rows = rowsOfKeys(reloadedKeys)
    if (rows.isNotEmpty()) reloadRows(rows, payload)
    if (!planned) reloadSectionIndex()
  }

  private fun rowsOfKeys(wanted: Set<String>): IntArray {
    if (wanted.isEmpty()) return IntArray(0)
    val rows = ArrayList<Int>()
    for ((row, key) in keys.withIndex()) if (key in wanted) rows.add(row)
    return rows.toIntArray()
  }

  /*
   * The data source already shows the new data. Read every key, work out the inserts, deletes
   * and moves against the keys held with the core's diffKeys, reload the rows whose content
   * version changed, and return what changed. Animates like any change with animatesChanges.
   */
  fun applyChanges(): SLKListChanges {
    val previousItems = itemKeysOf(keys, rowItems)
    val next = spareKeys
    readRowKeys(next)
    val nextItems = itemKeysOf(next, rowItems)
    val diff = SLKCore.diffKeys(previousItems, nextItems)

    // Rows that stayed but whose content version changed get reloaded.
    val reloaded = ArrayList<Int>()
    val reloadedKeys = HashSet<String>()
    val versioned = dataSource as? ContentVersions
    if (versioned != null) {
      val versions = HashMap<String, Long>(nextItems.size * 2)
      for ((item, key) in nextItems.withIndex()) {
        val version = versioned.contentVersionForItem(this, item)
        val known = contentVersions[key]
        if (known != null && known != version) {
          reloaded.add(item)
          reloadedKeys.add(key)
        }
        versions[key] = version
      }
      contentVersions = versions
    }

    applyRowKeys(next)
    val rows = rowsOfKeys(reloadedKeys)
    if (rows.isNotEmpty()) reloadRows(rows, null)
    reloadSectionIndex()

    var at = 0
    val deleted = IntArray(diff[at]) { diff[at + 1 + it] }
    at += 1 + deleted.size
    val inserted = IntArray(diff[at]) { diff[at + 1 + it] }
    at += 1 + inserted.size
    val moves = diff[at]
    val movedFrom = IntArray(moves) { diff[at + 1 + it * 2] }
    val movedTo = IntArray(moves) { diff[at + 2 + it * 2] }
    return SLKListChanges(deleted, inserted, movedFrom, movedTo, reloaded.toIntArray())
  }

  private fun itemKeysOf(rows: List<String>, items: IntArray?): List<String> {
    if (items == null) return ArrayList(rows)
    val result = ArrayList<String>(rows.size)
    for ((row, key) in rows.withIndex()) if (row < items.size && items[row] >= 0) result.add(key)
    return result
  }

  /*
   * Sorted, unique and inside 0 until limit. The core gets the same list the keys here used.
   */
  private fun validIndices(indices: IntArray, limit: Int): IntArray =
    indices.filter { it in 0 until limit }.distinct().sorted().toIntArray()

  /*
   * Calls block with the first and last position of each run of adjacent values in sorted.
   */
  private inline fun forEachRun(sorted: IntArray, block: (first: Int, last: Int) -> Unit) {
    var first = 0
    while (first < sorted.size) {
      var last = first
      while (last + 1 < sorted.size && sorted[last + 1] == sorted[last] + 1) ++last
      block(first, last)
      first = last + 1
    }
  }

  private fun commonPrefix(previous: List<String>, next: List<String>): Int {
    val limit = min(previous.size, next.size)
    var count = 0
    while (count < limit && previous[count] == next[count]) ++count
    return count
  }

  /*
   * Keys equal at the end of both lists, not reaching into the first start keys of either.
   */
  private fun commonSuffix(previous: List<String>, next: List<String>, start: Int): Int {
    val limit = min(previous.size, next.size) - start
    var count = 0
    while (count < limit && previous[previous.size - 1 - count] == next[next.size - 1 - count]) ++count
    return count
  }

  /*
   * The data changed. Sticky rows follow the sections, the selection drops removed rows and a
   * waiting saved position lands once its row is there.
   */
  private fun structureChanged() {
    ++structureVersion
    // An open row closes. One swiped all the way stays out while its removal runs.
    swipe.cell?.let { if (!swipe.isSwipedOut(it)) swipe.close(false) }
    if (stickyIndices.isNotEmpty() || stickySectionHeaders) updateStickyRows()
    if (selectedKeys.isNotEmpty()) selectedKeys.retainAll(HashSet(keys))
    restorePendingAnchor()
    invalidateFrame()
  }

  // endregion

  // region Sections

  internal fun itemForRow(row: Int): Int {
    val items = rowItems ?: return if (row in keys.indices) row else -1
    return if (row in items.indices) items[row] else -1
  }

  internal fun rowForItem(item: Int): Int {
    if (item < 0 || item >= itemCount) return -1
    return if (isSectioned) core.rowForItem(item) else item
  }

  val numberOfSections: Int get() = sectionCounts?.size ?: 1

  // The section of an item, or -1.
  fun sectionForItem(index: Int): Int {
    if (index < 0 || index >= itemCount) return -1
    return if (isSectioned) core.sectionForItem(index) else 0
  }

  // The item index a section's items start at, or -1.
  fun firstItemIndexInSection(section: Int): Int {
    if (!isSectioned) return if (section == 0) 0 else -1
    return core.firstItemInSection(section)
  }

  // The frame of a section's header in the list's scrolled coordinates, or null.
  fun rectForHeaderInSection(section: Int): RectF? {
    if (!isSectioned) return null
    return rowRectF(core.headerRow(section))
  }

  fun scrollToSection(section: Int, animated: Boolean = false) {
    val row = if (isSectioned) core.firstRowInSection(section) else if (section == 0 && keys.isNotEmpty()) 0 else -1
    if (row >= 0) scrollToRow(row, 0.0, animated)
  }

  private fun updateStickyRows() {
    val rows = ArrayList<Int>()
    for (item in stickyIndices) {
      val row = rowForItem(item)
      if (row >= 0) rows.add(row)
    }
    if (stickySectionHeaders && isSectioned) rows.addAll(core.headerRows().asList())
    val sorted = rows.distinct().sorted().toIntArray()
    if (sorted.contentEquals(stickyRows)) return
    stickyRows = sorted
    coreOrNull?.setStickyIndices(sorted)
    stickyGeometry = -1.0
    mountedLow = -1
  }

  private fun reloadSectionIndex() {
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
    if (scrollPhase() != SLKCore.PHASE_IDLE) userScrolled = true
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
    pass[SLKCore.PASS_OFFSET] = offset.toDouble()
    pass[SLKCore.PASS_WINDOW_ALONG] = windowAlong.toDouble()
    pass[SLKCore.PASS_WINDOW_CROSS] = windowCross.toDouble()
    pass[SLKCore.PASS_HEADER_SIZE] = headerSize.toDouble()
    pass[SLKCore.PASS_FOOTER_SIZE] = footerSize.toDouble()
    pass[SLKCore.PASS_PHASE] = scrollPhase().toDouble()
    pass[SLKCore.PASS_USER_SCROLLED] = if (userScrolled) 1.0 else 0.0
    pass[SLKCore.PASS_TRACKING] = if (gesture.isTracking) 1.0 else 0.0
    core.runPasses()
    userScrolled = false
    needsFrame = false
    applyPassResult(pass)
  }

  /*
   * The content size goes first. The offset write is then inside the scroll range.
   */
  private fun applyPassResult(pass: DoubleArray) {
    contentAlong = pass[SLKCore.PASS_OUT_CONTENT].roundToInt()
    val target = pass[SLKCore.PASS_OUT_OFFSET].roundToInt()
    if (target != offset) writeOffset(target)
    bandLow = pass[SLKCore.PASS_OUT_BAND_LOW]
    bandHigh = pass[SLKCore.PASS_OUT_BAND_HIGH]
    reachedStart = reachedStart || pass[SLKCore.PASS_OUT_REACHED_START] != 0.0
    reachedEnd = reachedEnd || pass[SLKCore.PASS_OUT_REACHED_END] != 0.0
    frameGeometry = pass[SLKCore.PASS_OUT_GEOMETRY]
    copyWindow(pass)
    if (pass[SLKCore.PASS_OUT_SETTLING] != 0.0) scheduleSettleFrame()
  }

  /*
   * Keep the window's frames on this side. Mount passes inside the band read only these.
   */
  private fun copyWindow(pass: DoubleArray) {
    val low = pass[SLKCore.PASS_OUT_WINDOW_LOW].toInt()
    val high = pass[SLKCore.PASS_OUT_WINDOW_HIGH].toInt()
    val geometry = pass[SLKCore.PASS_OUT_GEOMETRY]
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
      layoutPass()
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
  private fun applyState(cell: SLKListCell, key: String) {
    val selected = cell.index >= 0 && key in selectedKeys
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

  private fun placeCell(cell: SLKListCell, index: Int) {
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
    val items = itemsOfRows(packed, 1, prefetchCount)
    val cancelCount = packed[1 + prefetchCount]
    val cancelled = itemsOfRows(packed, 2 + prefetchCount, cancelCount)
    if (items.isNotEmpty()) prefetch.prefetchItems(this, items)
    if (cancelled.isNotEmpty()) prefetch.cancelPrefetchingForItems(this, cancelled)
  }

  private fun itemsOfRows(rows: IntArray, from: Int, count: Int): IntArray {
    val items = IntArray(count)
    var found = 0
    for (at in from until from + count) {
      val item = itemForRow(rows[at])
      if (item >= 0) items[found++] = item
    }
    return items.copyOf(found)
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
        val section = place shr 2
        val size = if ((place and 3) == SLKCore.ROW_FOOTER) sections.sizeForFooterInSection(this, section, cross)
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
   * Position in stickyRows of the last sticky row starting at or above the offset, or -1.
   */
  private fun activeStickyPosition(offset: Double): Int {
    if (stickyRows.isEmpty()) return -1
    refreshStickyFrames()
    var found = -1
    var low = 0
    var high = stickyRows.size
    while (low < high) {
      val mid = (low + high) ushr 1
      if (stickyRows[mid] < keys.size && stickyFrames[mid * 2] <= offset) {
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
    if (next < stickyRows.size && stickyRows[next] < keys.size) {
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

  private fun unpinCell(cell: SLKListCell) {
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
    if (showsSeparators && child is SLKListCell && child.visibility == VISIBLE && child.index >= 0) {
      drawSeparator(canvas, child)
    }
    return more
  }

  private fun drawSeparator(canvas: Canvas, cell: SLKListCell) {
    val row = cell.row
    if (numberOfColumns > 1 || row < 0) return
    val follows = rowSeparators?.let { row < it.size && it[row] } ?: (row + 1 < keys.size)
    if (!follows || delegate?.showsSeparatorAfterItem(this, cell.index) == false) return
    val x = cell.left + cell.translationX
    val y = cell.top + cell.translationY
    if (horizontal) {
      separatorRect.set(x + cell.width - separatorThickness, y + separatorInsetStart, x + cell.width, y + cell.height - separatorInsetEnd)
    } else {
      separatorRect.set(x + separatorInsetStart, y + cell.height - separatorThickness, x + cell.width - separatorInsetEnd, y + cell.height)
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
  internal fun cellAt(x: Float, y: Float): SLKListCell? {
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

  // The visible item cell under a point, not a section header or footer.
  internal fun itemCellAt(x: Float, y: Float): SLKListCell? = cellAt(x, y)?.takeIf { it.index >= 0 }

  internal fun handleTap(x: Float, y: Float) {
    if (swipe.isOpen || swipe.closingTouch || hasHeldRow) return
    val cell = itemCellAt(x, y) ?: return
    if (allowsSelection) userSelected(cell)
  }

  /*
   * Show a row's context menu. Returns whether the delegate gave one.
   */
  internal fun showMenu(cell: SLKListCell): Boolean {
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

  /*
   * A tap on a row. Multiple selection toggles it, single selection moves to it.
   */
  private fun userSelected(cell: SLKListCell) {
    val index = cell.index
    val key = keys.getOrNull(cell.row) ?: return
    if (allowsMultipleSelection && key in selectedKeys) {
      selectedKeys.remove(key)
      cell.setSelected(false, true)
      delegate?.didDeselectItem(this, index)
      return
    }
    if (delegate?.shouldSelectItem(this, index) == false) return
    if (!allowsMultipleSelection) {
      for (previous in selectedKeys.toList()) {
        if (previous == key) continue
        selectedKeys.remove(previous)
        mounted[previous]?.setSelected(false, true)
        val previousIndex = keys.indexOf(previous).let(::itemForRow)
        if (previousIndex >= 0) delegate?.didDeselectItem(this, previousIndex)
      }
    }
    selectedKeys.add(key)
    if (!cell.isSelected) cell.setSelected(true, true)
    delegate?.didSelectItem(this, index)
  }

  val selectedIndices: IntArray
    get() {
      if (selectedKeys.isEmpty()) return IntArray(0)
      val items = ArrayList<Int>()
      for ((row, key) in keys.withIndex()) {
        if (key in selectedKeys) itemForRow(row).takeIf { it >= 0 }?.let(items::add)
      }
      return items.toIntArray()
    }

  // Select an item without delegate calls.
  fun selectItem(index: Int, animated: Boolean = false) {
    val key = keys.getOrNull(rowForItem(index)) ?: return
    if (!allowsSelection) return
    if (!allowsMultipleSelection) {
      for (previous in selectedKeys) if (previous != key) mounted[previous]?.setSelected(false, animated)
      selectedKeys.clear()
    }
    selectedKeys.add(key)
    mounted[key]?.let { if (!it.isSelected) it.setSelected(true, animated) }
  }

  fun deselectItem(index: Int, animated: Boolean = false) {
    val key = keys.getOrNull(rowForItem(index)) ?: return
    if (selectedKeys.remove(key)) mounted[key]?.setSelected(false, animated)
  }

  private fun clearSelection() {
    for (key in selectedKeys) mounted[key]?.setSelected(false, false)
    selectedKeys.clear()
  }

  /*
   * A finger resting on a selectable row highlights it after the tap timeout, unless it moves.
   */
  internal fun highlightDown(x: Float, y: Float) {
    cancelHighlight()
    if (!allowsSelection || hasHeldRow || swipe.isOpen || swipe.closingTouch) return
    highlightX = x
    highlightY = y
    postDelayed(highlightRunnable, ViewConfiguration.getTapTimeout().toLong())
  }

  private fun highlightPending() {
    val cell = itemCellAt(highlightX, highlightY) ?: return
    if (delegate?.shouldHighlightItem(this, cell.index) == false) return
    cell.setHighlighted(true, false)
    highlightedCell = cell
  }

  internal fun cancelHighlight() {
    removeCallbacks(highlightRunnable)
    highlightedCell?.let { if (it.highlighted) it.setHighlighted(false, true) }
    highlightedCell = null
  }

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
    val now = android.os.SystemClock.uptimeMillis()
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
    cancelHighlight()
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
    cancelHighlight()
    drag.cancelPress()
    if (swipe.isOpen) swipe.close(true)
  }

  /*
   * Where a fling that would stop at target rests instead.
   */
  internal fun snapTarget(target: Int): Int =
    if (snapToItem) core.nearestSnapOffset(target.toDouble()).roundToInt() else target

  // Whether a drag past the start pulls the refresh spinner.
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

  fun cellForItem(index: Int): SLKListCell? {
    return mountedCell(rowForItem(index))?.takeIf { it.visibility == VISIBLE }
  }

  val visibleCells: List<SLKListCell>
    get() {
      val low = offset + leadingPadding
      val high = low + windowAlong
      return mounted.values
        .filter { it.visibility == VISIBLE && it.index >= 0 && (if (horizontal) it.right > low && it.left < high else it.bottom > low && it.top < high) }
        .sortedBy { it.row }
    }

  // Size of the content along the scroll axis, header and footer included, in pixels.
  val contentSize: Int get() = contentAlong

  // The items among the rows overlapping the viewport, low to high, or null before the first layout.
  val visibleRange: IntRange?
    get() {
      val packed = core.visibleRange()
      if (packed < 0) return null
      var low = -1
      var high = -1
      for (row in (packed shr 32).toInt()..(packed and 0xffffffffL).toInt()) {
        val item = itemForRow(row)
        if (item < 0) continue
        if (low < 0) low = item
        high = item
      }
      return if (low < 0) null else low..high
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
  val anchorState: SLKAnchorState?
    get() {
      val current = coreOrNull ?: return pendingAnchor
      val out = DoubleArray(1)
      val key = current.anchor(offset.toDouble(), out) ?: return pendingAnchor
      return SLKAnchorState(key, out[0].toFloat())
    }

  /*
   * Land the saved row the same distance in again, now or once a reload brings its key.
   */
  fun restoreAnchorState(state: SLKAnchorState) {
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
    state.classLoader = SLKAnchorState::class.java.classLoader
    super.onRestoreInstanceState(state.getParcelable(STATE_SUPER))
    state.getParcelable<SLKAnchorState>(STATE_ANCHOR)?.let(::restoreAnchorState)
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
    val target = min(max(offset + direction * windowAlong, 0), maxOffset)
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
      animateCommand(target.roundToInt(), SLKCore.LANDING_INDEX, row, viewPosition)
      return
    }
    stopScrolling()
    core.scrollToIndex(row, viewPosition)
    runCommandNow()
  }

  fun scrollToStart(animated: Boolean = false) {
    if (animated) {
      // The header shows at the very start, which landing on row 0 would scroll past.
      animateCommand(0, SLKCore.LANDING_START)
      return
    }
    stopScrolling()
    core.scrollToStart()
    runCommandNow()
  }

  fun scrollToEnd(animated: Boolean = false) {
    if (animated) {
      animateCommand(maxOffset, SLKCore.LANDING_END)
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

  private fun runCommandNow() {
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
    const val STATE_SUPER = "SLKListView.super"
    const val STATE_ANCHOR = "SLKListView.anchor"
  }
}

/*
 * The cell of a section header or footer the data source gives only a title for.
 */
internal class SLKSectionTitleCell(context: Context, identifier: String) : SLKListCell(context, identifier) {
  companion object {
    const val HEADER = "SLKSectionHeader"
    const val FOOTER = "SLKSectionFooter"
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
    if (!footer) label.setTypeface(label.typeface, android.graphics.Typeface.BOLD)
    label.setPadding((16 * density).roundToInt(), (6 * density).roundToInt(), (16 * density).roundToInt(), (6 * density).roundToInt())
    label.minHeight = (28 * density).roundToInt()
    label.gravity = android.view.Gravity.CENTER_VERTICAL
    if (!footer) setBackgroundColor(if (night) 0xFF1C1C1E.toInt() else 0xFFF2F2F7.toInt())
    addView(label, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
  }
}
