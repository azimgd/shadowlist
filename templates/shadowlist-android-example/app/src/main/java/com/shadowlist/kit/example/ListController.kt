package com.shadowlist.kit.example

import android.content.Context
import android.content.res.Resources
import android.graphics.Canvas
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.StaggeredGridLayoutManager
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView

/*
 * How a new set of rows relates to the shown one.
 */
sealed class RowChange {
  object Reset : RowChange()
  object Update : RowChange()
  class Prepend(val count: Int) : RowChange()
  class Append(val count: Int) : RowChange()
}

/*
 * One list engine behind the screens. The rows and their views are the same for every engine.
 */
interface ListBackend {
  val view: View
  fun setRows(rows: List<Row>, change: RowChange)
  fun setStickyIndices(indices: IntArray)
  fun setFooter(footer: View?)
  fun scrollToItem(index: Int, animated: Boolean)
}

/*
 * Builds the engine a screen asked for and keeps the precomputed layouts of its rows.
 */
class ListController(context: Context, engine: Engine, inverted: Boolean = false, columns: Int = 1) {
  val layouts = LayoutCache()
  private val columnWidth = Resources.getSystem().displayMetrics.widthPixels / columns
  /*
   * Whether row views get layouts computed ahead off the UI thread.
   */
  val precomputed = !engine.selfSizing

  val backend: ListBackend = if (engine.isShadowList) {
    ShadowListBackend(context, this, inverted, columns)
  } else {
    RecyclerBackend(context, this, inverted, columns)
  }

  var rows: List<Row> = emptyList()
    private set

  /*
   * How long the previous backend update took on the UI thread, for the cost scenario.
   */
  var previousUpdateNanos = 0L
    private set

  fun setRows(rows: List<Row>, change: RowChange) {
    this.rows = rows
    if (precomputed) layouts.prefetch(rows, columnWidth, if (rows.isEmpty()) 0 else rows.size - 1)
    val start = System.nanoTime()
    backend.setRows(rows, change)
    previousUpdateNanos = System.nanoTime() - start
  }

  /*
   * The layout an engine hands the row view, or null to let the view lay itself out.
   */
  fun layoutFor(row: Row, width: Int): RowLayout? = if (precomputed) layouts.layout(row, width) else null
}

// region ShadowListKitListView

private class ShadowListBackend(
  context: Context,
  private val controller: ListController,
  inverted: Boolean,
  columns: Int,
) : ListBackend, ShadowListKitListView.Delegate {
  private val list = ShadowListKitListView(context)
  private val registered = HashSet<String>()

  override val view: View get() = list

  init {
    list.setBackgroundColor(Theme.background)
    list.inverted = inverted
    list.numberOfColumns = columns
    list.delegate = this
    list.dataSource = if (controller.precomputed) SizedSource() else Source()
    list.animatesChanges = Settings.animate
    if (Settings.padding > 0) {
      // Rows scroll under the padding like under translucent bars.
      val padding = Settings.padding.dp
      list.setPadding(padding, padding * 3, padding, padding * 3)
      list.clipToPadding = false
    }
  }

  /*
   * Rows by index for the list. Cells come from the row's own view type.
   */
  private open inner class Source : ShadowListKitListView.DataSource {
    override fun numberOfItems(listView: ShadowListKitListView) = controller.rows.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int) = controller.rows[index].key

    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
      val row = controller.rows[index]
      if (registered.add(row.viewType)) listView.registerCell(row.viewType) { row.makeView(it) }
      val cell = listView.dequeueReusableCell<RowView>(row.viewType)
      cell.configure(row, controller.layouts.cached(row, cellWidth()))
      return cell
    }

    private fun cellWidth(): Int = (list.width - list.paddingLeft - list.paddingRight) / list.numberOfColumns
  }

  /*
   * Sizes from the precomputed layouts, which the list never measures through a cell.
   */
  private inner class SizedSource : Source(), ShadowListKitListView.Sizing {
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int =
      controller.layouts.layout(controller.rows[index], crossSize).height
  }

  override fun setRows(rows: List<Row>, change: RowChange) = when (change) {
    is RowChange.Prepend -> list.insertItems(IntArray(change.count) { it })
    is RowChange.Append -> list.insertItems(IntArray(change.count) { rows.size - change.count + it })
    else -> list.reloadData()
  }

  override fun setStickyIndices(indices: IntArray) {
    list.stickyIndices = indices
  }

  override fun setFooter(footer: View?) {
    list.footerView = footer
  }

  override fun scrollToItem(index: Int, animated: Boolean) = list.scrollToItem(index, 0.0, animated)
}

// endregion

// region RecyclerView

private class RecyclerBackend(
  context: Context,
  private val controller: ListController,
  inverted: Boolean,
  private val columns: Int,
) : ListBackend {
  private val recycler = RecyclerView(context)
  private val adapter = RowAdapter()
  private val sticky = StickyHeaders(recycler, controller)
  private val viewTypes = HashMap<String, Int>()
  private val viewTypeRows = HashMap<Int, Row>()
  private var footer: View? = null

  override val view: View get() = recycler

  init {
    recycler.setBackgroundColor(Theme.background)
    recycler.setHasFixedSize(true)
    recycler.layoutManager = if (columns > 1) {
      StaggeredGridLayoutManager(columns, StaggeredGridLayoutManager.VERTICAL)
    } else {
      LinearLayoutManager(context).apply { stackFromEnd = inverted }
    }
    adapter.setHasStableIds(true)
    recycler.adapter = adapter
    recycler.addItemDecoration(sticky)
  }

  private inner class RowAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    override fun getItemCount() = controller.rows.size + (if (footer != null) 1 else 0)
    override fun getItemId(position: Int): Long =
      controller.rows.getOrNull(position)?.key?.hashCode()?.toLong() ?: Long.MAX_VALUE

    override fun getItemViewType(position: Int): Int {
      val row = controller.rows.getOrNull(position) ?: return FOOTER_TYPE
      return viewTypes.getOrPut(row.viewType) { viewTypes.size }.also { viewTypeRows[it] = row }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
      val view = if (viewType == FOOTER_TYPE) FrameLayout(parent.context) else viewTypeRows.getValue(viewType).makeView(parent.context)
      view.layoutParams = RecyclerView.LayoutParams(-1, -2)
      return object : RecyclerView.ViewHolder(view) {}
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
      val row = controller.rows.getOrNull(position)
      if (row == null) {
        bindFooter(holder.itemView as FrameLayout)
        return
      }
      val width = recycler.width / columns
      val layout = controller.layoutFor(row, width)
      holder.itemView.layoutParams.height = layout?.height ?: ViewGroup.LayoutParams.WRAP_CONTENT
      (holder.itemView as RowView).configure(row, layout)
    }
  }

  private fun bindFooter(holder: FrameLayout) {
    val footer = footer ?: return
    if (footer.parent === holder) return
    (footer.parent as? ViewGroup)?.removeView(footer)
    holder.addView(footer)
  }

  /*
   * Inserts go through the adapter's range calls, which keep the first visible row in place.
   * A regroup has no finer call than a full change.
   */
  override fun setRows(rows: List<Row>, change: RowChange) {
    when (change) {
      is RowChange.Prepend -> adapter.notifyItemRangeInserted(0, change.count)
      is RowChange.Append -> adapter.notifyItemRangeInserted(rows.size - change.count, change.count)
      else -> {
        @Suppress("NotifyDataSetChanged")
        adapter.notifyDataSetChanged()
      }
    }
    sticky.rowsChanged()
  }

  override fun setStickyIndices(indices: IntArray) = sticky.setIndices(indices)

  override fun setFooter(footer: View?) {
    this.footer = footer
  }

  override fun scrollToItem(index: Int, animated: Boolean) {
    if (animated) recycler.smoothScrollToPosition(index) else recycler.scrollToPosition(index)
  }

  companion object {
    const val FOOTER_TYPE = Int.MAX_VALUE
  }
}

/*
 * RecyclerView has no sticky headers. The usual item decoration draws the current section's
 * header over the rows and pushes it up when the next header arrives.
 */
private class StickyHeaders(private val recycler: RecyclerView, private val controller: ListController) :
  RecyclerView.ItemDecoration() {
  private var indices = IntArray(0)
  private var shownIndex = -1
  private var header: RowView? = null

  fun setIndices(indices: IntArray) {
    this.indices = indices
    rowsChanged()
  }

  fun rowsChanged() {
    shownIndex = -1
  }

  override fun onDrawOver(canvas: Canvas, parent: RecyclerView, state: RecyclerView.State) {
    val first = parent.getChildAt(0)?.let { parent.getChildAdapterPosition(it) } ?: return
    val active = activeIndex(first)
    if (active < 0) return
    val view = headerFor(active)
    canvas.save()
    canvas.translate(0f, pushUp(active, view.height).toFloat())
    view.draw(canvas)
    canvas.restore()
  }

  private fun activeIndex(first: Int): Int {
    if (first < 0) return -1
    var found = -1
    for (index in indices) if (index <= first) found = index else break
    return found
  }

  /*
   * The header view, configured for the row and laid out by hand. It is never attached.
   */
  private fun headerFor(index: Int): RowView {
    val row = controller.rows[index]
    val view = header ?: row.makeView(recycler.context).also { header = it }
    if (index == shownIndex) return view
    shownIndex = index
    view.configure(row, controller.layoutFor(row, recycler.width))
    view.measure(
      View.MeasureSpec.makeMeasureSpec(recycler.width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
    view.layout(0, 0, recycler.width, view.measuredHeight)
    return view
  }

  private fun pushUp(active: Int, height: Int): Int {
    val next = indices.firstOrNull { it > active } ?: return 0
    val nextView = recycler.findViewHolderForAdapterPosition(next)?.itemView ?: return 0
    return if (nextView.top < height) nextView.top - height else 0
  }
}

// endregion
