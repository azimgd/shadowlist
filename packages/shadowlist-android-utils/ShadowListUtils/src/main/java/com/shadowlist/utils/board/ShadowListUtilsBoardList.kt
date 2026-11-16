package com.shadowlist.utils

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView
import java.util.UUID
import kotlin.math.ceil

/*
 * Application-owned columns on one isolated drag board. onMoveItem requests a change; update
 * columns in that callback to accept it. Header and footer views stay outside the draggable rows.
 */
class ShadowListUtilsBoardList<Item>(
  context: Context,
  private val keyExtractor: (Item) -> String,
  rowForItem: (Item) -> ShadowListUtilsRow,
  theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
  idPrefix: String? = null,
) : FrameLayout(context) {
  private val boardId = "${idPrefix ?: "board"}-${UUID.randomUUID()}"
  private val outer = ShadowListKitListView(context)
  private var measuredColumnWidth = -1
  private var columnWidthPx = 280
  private var columnGapPx = 12
  private val columnViews = LinkedHashMap<String, ColumnView>()
  private var storedColumns = emptyList<ShadowListUtilsBoardColumn<Item>>()
  private val layoutValues = DoubleArray(ShadowListUtilsCore.BOARD_LAYOUT_SLOTS)

  var columns: List<ShadowListUtilsBoardColumn<Item>>
    get() = storedColumns
    set(value) = setColumns(value, false)

  fun setColumns(columns: List<ShadowListUtilsBoardColumn<Item>>, animated: Boolean) {
    require(columns.map { it.key }.toSet().size == columns.size) { "Board column keys must be unique" }
    val previousLast = storedColumns.lastOrNull()?.key
    storedColumns = columns
    for (key in columnViews.keys.toList()) if (columns.none { it.key == key }) {
      columnViews.remove(key)?.let { (it.parent as? android.view.ViewGroup)?.removeView(it) }
    }
    for (column in columns) {
      val view = columnViews[column.key] ?: makeColumn(column.key)
      view.list.theme = theme
      view.list.dragBoardId = boardId
      view.list.dragListId = listId(column.key)
      view.list.dragListLabel = column.title
      view.list.reorderEnabled = onMoveItem != null
      val rows = column.items.map { item -> rowForItem(item).also { require(it.key == keyExtractor(item)) { "A board row must use its item's key" } } }
      view.list.setRows(rows, animated)
      view.list.setBackgroundColor(android.graphics.Color.TRANSPARENT)
      val renderer = renderColumnHeader
      val header = if (renderer != null) renderer(column) else ShadowListUtilsListHeader(context, column.title, column.items.size.toString(), theme).apply {
        contentDescription = labels.columnSummary(column.title, column.items.size)
      }
      view.show(header, renderColumnFooter?.invoke(column))
      view.empty.text = if (onMoveItem == null) labels.emptyColumnReadOnly else labels.emptyColumn
      view.empty.visibility = if (column.items.isEmpty()) VISIBLE else GONE
      view.empty.setTextColor(theme.colors.secondaryLabel)
      view.background = GradientDrawable().apply { setColor(theme.colors.elevated); cornerRadius = theme.radius.lg.pxf }
      view.clipToOutline = true
    }
    outer.reloadData()
    if (previousLast != columns.lastOrNull()?.key) {
      val changed = setOfNotNull(previousLast, columns.lastOrNull()?.key)
      outer.reloadItems(columns.indices.filter { columns[it].key in changed }.toIntArray())
    }
    requestLayout()
  }

  var onMoveItem: ((ShadowListUtilsBoardMoveInfo<Item>) -> Unit)? = null
    set(value) { field = value; setColumns(storedColumns, false) }
  var rowForItem: (Item) -> ShadowListUtilsRow = rowForItem
    set(value) { field = value; setColumns(storedColumns, false) }
  var renderColumnHeader: ((ShadowListUtilsBoardColumn<Item>) -> View?)? = null
    set(value) { field = value; setColumns(storedColumns, false) }
  var renderColumnFooter: ((ShadowListUtilsBoardColumn<Item>) -> View?)? = null
    set(value) { field = value; setColumns(storedColumns, false) }
  var labels = ShadowListUtilsBoardLabels()
    set(value) { field = value; setColumns(storedColumns, false) }
  var theme: ShadowListUtilsTheme = theme
    set(value) { field = value; setColumns(storedColumns, false) }
  var columnWidth: Float? = null
    set(value) { field = value; requestLayout() }
  var sidePadding: Float? = null
    set(value) { field = value; requestLayout() }
  var columnHeight = ShadowListUtilsBoardColumnHeight.FILL
    set(value) { field = value; requestLayout() }
  var minColumnHeight: Float = 120f
    set(value) { field = value; requestLayout() }
  var maxColumnHeight: Float? = null
    set(value) { field = value; requestLayout() }
  var estimatedItemHeight: Float = 64f
    set(value) { field = value; requestLayout() }

  init {
    outer.horizontal = true
    outer.clipToPadding = false
    outer.snapToItem = true
    outer.allowsSelection = false
    outer.isHorizontalScrollBarEnabled = false
    outer.registerCell("board-column") { ColumnCell(it) }
    outer.dataSource = object : ShadowListKitListView.DataSource, ShadowListKitListView.Sizing {
      override fun numberOfItems(listView: ShadowListKitListView): Int = storedColumns.size
      override fun keyForItem(listView: ShadowListKitListView, index: Int): String = storedColumns[index].key
      override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int =
        columnWidthPx + if (index + 1 < storedColumns.size) columnGapPx else 0
      override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
        val cell = listView.dequeueReusableCell("board-column") as ColumnCell
        cell.show(columnViews.getValue(storedColumns[index].key), columnWidthPx)
        return cell
      }
    }
    addView(outer, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
  }

  private fun listId(key: String): String = "$boardId/$key"

  private fun makeColumn(key: String): ColumnView {
    val view = ColumnView(context, theme)
    view.list.onMoveWithin = { source, destination -> moveWithin(key, source, destination) }
    view.list.onMoveToList = { source, listId, destination, destinationKey -> moveBetween(key, source, listId, destination, destinationKey) }
    view.list.contentChanged = { requestLayout() }
    columnViews[key] = view
    return view
  }

  private fun moveWithin(key: String, source: Int, destination: Int) {
    val column = storedColumns.firstOrNull { it.key == key } ?: return
    val item = column.items.getOrNull(source) ?: return
    val itemKey = keyExtractor(item)
    val remaining = column.items.filterNot { keyExtractor(it) == itemKey }
    val destinationKey = remaining.getOrNull(destination)?.let(keyExtractor) ?: ""
    onMoveItem?.invoke(ShadowListUtilsBoardMoveInfo(item, key, key, destinationKey, destination))
  }

  private fun moveBetween(key: String, source: Int, destinationListId: String, destination: Int, destinationKey: String) {
    val column = storedColumns.firstOrNull { it.key == key } ?: return
    val item = column.items.getOrNull(source) ?: return
    val target = storedColumns.firstOrNull { listId(it.key) == destinationListId } ?: return
    onMoveItem?.invoke(ShadowListUtilsBoardMoveInfo(item, key, target.key, destinationKey, destination))
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val density = resources.displayMetrics.density.toDouble()
    val width = MeasureSpec.getSize(widthMeasureSpec)
    ShadowListUtilsCore.boardColumnLayout(width / density, columnWidth?.toDouble() ?: Double.NaN, sidePadding?.toDouble() ?: Double.NaN, layoutValues)
    columnWidthPx = layoutValues[ShadowListUtilsCore.BOARD_COLUMN_WIDTH].toFloat().px
    val padding = layoutValues[ShadowListUtilsCore.BOARD_SIDE_PADDING].toFloat().px
    val interval = layoutValues[ShadowListUtilsCore.BOARD_SNAP_INTERVAL].toFloat().px
    columnGapPx = interval - columnWidthPx
    outer.setPadding(padding, 0, padding, 0)
    val heights = storedColumns.map { column ->
      val measured = columnViews[column.key]!!.list.contentSize
      if (measured > 0) measured / density else column.items.size * estimatedItemHeight.toDouble()
    }.toDoubleArray()
    val body = ShadowListUtilsCore.boardColumnHeight(heights, minColumnHeight.toDouble(), maxColumnHeight?.toDouble() ?: resources.displayMetrics.heightPixels / density * 0.6).toFloat().px
    var tallest = 0
    for (column in storedColumns) {
      val view = columnViews[column.key]!!
      val exactWidth = MeasureSpec.makeMeasureSpec(columnWidthPx, MeasureSpec.EXACTLY)
      val unbounded = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED)
      view.header?.measure(exactWidth, unbounded)
      view.footer?.measure(exactWidth, unbounded)
      val header = view.header?.measuredHeight ?: 0
      val footer = view.footer?.measuredHeight ?: 0
      val bodyHeight = if (columnHeight == ShadowListUtilsBoardColumnHeight.CONTENT) body else (MeasureSpec.getSize(heightMeasureSpec) - header - footer).coerceAtLeast(0)
      view.body.layoutParams = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, bodyHeight)
      view.list.scrollEnabled = columnHeight == ShadowListUtilsBoardColumnHeight.FILL || view.list.contentSize > bodyHeight
      view.layoutParams = LayoutParams(columnWidthPx, header + bodyHeight + footer)
      tallest = maxOf(tallest, header + bodyHeight + footer)
    }
    val height = if (columnHeight == ShadowListUtilsBoardColumnHeight.CONTENT) resolveSize(tallest, heightMeasureSpec) else MeasureSpec.getSize(heightMeasureSpec)
    /*
     * React Native keeps every board column attached. The outer host covers the whole column
     * strip with its existing overscan settings; each inner list keeps its bounded row range.
     */
    val strip = storedColumns.size * interval + padding * 2
    val coverage = maxOf(1.0, ceil(strip.toDouble() / maxOf(1, width)) + 1)
    if (outer.overscan != coverage) outer.overscan = coverage
    if (outer.mountOverscan != coverage) outer.mountOverscan = coverage
    if (measuredColumnWidth != columnWidthPx) {
      measuredColumnWidth = columnWidthPx
      outer.reloadItems(IntArray(storedColumns.size) { it })
    }
    outer.measure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
    setMeasuredDimension(width, height)
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    outer.layout(0, 0, right - left, bottom - top)
  }

  internal fun listForColumn(key: String): ShadowListUtilsList? = columnViews[key]?.list

  private class ColumnCell(context: Context) : ShadowListKitListCell(context, "board-column") {
    private var column: View? = null
    private var columnWidth = 0
    fun show(column: View, width: Int) {
      if (this.column !== column) {
        this.column?.takeIf { it.parent === this }?.let(::removeView)
        (column.parent as? android.view.ViewGroup)?.removeView(column)
        this.column = column
        addView(column)
      }
      columnWidth = width
      requestLayout()
    }
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
      val width = MeasureSpec.getSize(widthMeasureSpec)
      val height = MeasureSpec.getSize(heightMeasureSpec)
      column?.measure(MeasureSpec.makeMeasureSpec(columnWidth, MeasureSpec.EXACTLY),
        MeasureSpec.makeMeasureSpec(column?.layoutParams?.height?.coerceAtLeast(0) ?: height, MeasureSpec.EXACTLY))
      setMeasuredDimension(width, height)
    }
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
      column?.let { it.layout(0, 0, columnWidth, it.measuredHeight) }
    }
    override fun prepareForReuse() {
      column?.takeIf { it.parent === this }?.let(::removeView)
      column = null
      super.prepareForReuse()
    }
  }

  private class ColumnView(context: Context, theme: ShadowListUtilsTheme) : LinearLayout(context) {
    val list = ColumnList(context, theme)
    val body = FrameLayout(context)
    val empty = TextView(context).apply { gravity = Gravity.CENTER; isClickable = false; importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO }
    var header: View? = null
      private set
    var footer: View? = null
      private set
    init {
      orientation = VERTICAL
      list.allowsSelection = false
      body.addView(list, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
      body.addView(empty, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, 48f.px, Gravity.TOP).apply { topMargin = 12f.px })
      addView(body)
    }
    fun show(header: View?, footer: View?) {
      if (this.header !== header) { this.header?.let(::removeView); this.header = header; header?.let { addView(it, 0) } }
      if (this.footer !== footer) { this.footer?.let(::removeView); this.footer = footer; footer?.let(::addView) }
    }
  }

  private class ColumnList(context: Context, theme: ShadowListUtilsTheme) : ShadowListUtilsList(context, theme) {
    var onMoveWithin: ((Int, Int) -> Unit)? = null
    var onMoveToList: ((Int, String, Int, String) -> Unit)? = null
    var contentChanged: (() -> Unit)? = null
    private var previousContentSize = 0
    override fun didMoveItem(sourceIndex: Int, destinationIndex: Int) { onMoveWithin?.invoke(sourceIndex, destinationIndex) }
    override fun didMoveItemToList(sourceIndex: Int, destinationListId: String, destinationIndex: Int, destinationKey: String) {
      onMoveToList?.invoke(sourceIndex, destinationListId, destinationIndex, destinationKey)
    }
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
      super.onLayout(changed, left, top, right, bottom)
      if (previousContentSize != contentSize) { previousContentSize = contentSize; contentChanged?.invoke() }
    }
  }
}
