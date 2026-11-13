package com.shadowlist.kit.example

import android.content.Context
import android.content.res.Resources
import android.graphics.Canvas
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.StaggeredGridLayoutManager
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.utils.ShadowListUtilsLayoutCache
import com.shadowlist.utils.ShadowListUtilsList
import com.shadowlist.utils.ShadowListUtilsRow
import com.shadowlist.utils.ShadowListUtilsRowLayout
import com.shadowlist.utils.ShadowListUtilsRowView
import java.util.WeakHashMap
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger

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
 * One list engine behind the screens. The rows and their views are the library's for every engine.
 */
interface ListBackend {
  val view: View
  fun setRows(rows: List<ShadowListUtilsRow>, change: RowChange)
  fun setStickyIndices(indices: IntArray)
  fun setFooter(footer: View?)
  fun scrollToItem(index: Int, animated: Boolean)
}

/*
 * Builds the engine a screen asked for. The sl engine is the screen's own template list from the
 * library. The other engines host the same library rows: ShadowListKitListView measuring each
 * cell, and RecyclerView with and without layouts computed ahead.
 */
class ListController(
  context: Context,
  engine: Engine,
  inverted: Boolean = false,
  columns: Int = 1,
  makeTemplate: () -> ShadowListUtilsList,
) {
  /*
   * The layouts of the rows for the engines that are not the template, which keeps its own.
   */
  val layouts = ShadowListUtilsLayoutCache()
  private val columnWidth = Resources.getSystem().displayMetrics.widthPixels / columns
  private val executor = Executors.newSingleThreadExecutor()
  private val main = Handler(Looper.getMainLooper())
  private val generation = AtomicInteger()

  /*
   * Whether row views get layouts computed ahead off the UI thread.
   */
  val precomputed = !engine.selfSizing

  /*
   * The screen's template list when the engine is sl, else null.
   */
  val template: ShadowListUtilsList? = if (engine == Engine.SHADOWLIST) makeTemplate() else null

  val backend: ListBackend = when {
    template != null -> TemplateBackend(template, this)
    engine.isShadowList -> SelfSizingBackend(context, this, inverted, columns)
    else -> RecyclerBackend(context, this, inverted, columns)
  }

  var rows: List<ShadowListUtilsRow> = emptyList()
    private set

  /*
   * How long the previous backend update took on the UI thread, for the cost scenario.
   */
  var previousUpdateNanos = 0L
    private set

  /*
   * Set by the cost scenario while it times updates. Layout work would compete with them.
   */
  @Volatile var prefetchPaused = false

  fun setRows(rows: List<ShadowListUtilsRow>, change: RowChange) {
    this.rows = rows
    if (precomputed && template == null) precompute(rows)
    val start = System.nanoTime()
    backend.setRows(rows, change)
    previousUpdateNanos = System.nanoTime() - start
  }

  /*
   * The template list took new items through its own setter in work. Its rows are the rows now.
   */
  fun update(template: ShadowListUtilsList, work: () -> Unit) {
    val start = System.nanoTime()
    work()
    previousUpdateNanos = System.nanoTime() - start
    rows = template.rows
  }

  /*
   * Lay out every row off the UI thread, the last ones first. The bench compares the engines on
   * sizes known ahead, and the cache's own prefetch stops a few hundred rows from where it starts.
   */
  private fun precompute(rows: List<ShadowListUtilsRow>) {
    if (prefetchPaused) return
    val current = generation.incrementAndGet()
    executor.execute {
      for (index in rows.indices.reversed()) {
        if (generation.get() != current) return@execute
        layouts.layout(rows[index], columnWidth)
      }
    }
  }

  /*
   * Run work on the main thread once the layouts queued so far are made.
   */
  fun whenIdle(work: () -> Unit) {
    executor.execute { main.post(work) }
  }

  /*
   * The layout an engine hands the row view, or null to let the view lay itself out.
   */
  fun layoutFor(row: ShadowListUtilsRow, width: Int): ShadowListUtilsRowLayout? =
    if (precomputed) layouts.layout(row, width) else null

  /*
   * Shows a just bound row view Settings.drawDelayMs late. A newer bind replaces the token.
   */
  fun delayDraw(view: View) {
    if (Settings.drawDelayMs <= 0) return
    val token = Any()
    drawTokens[view] = token
    view.alpha = 0f
    view.postDelayed({ if (drawTokens[view] === token) view.alpha = 1f }, Settings.drawDelayMs)
  }

  private val drawTokens = WeakHashMap<View, Any>()
}

/*
 * List padding from SLPadding. Rows scroll under it like under translucent bars.
 */
private fun applyPadding(list: ShadowListKitListView) {
  if (Settings.padding <= 0) return
  val padding = Settings.padding.dp
  list.setPadding(padding, padding * 3, padding, padding * 3)
  list.clipToPadding = false
}

// region Template

/*
 * The library's template list. It is its own data source and delegate, and pins its own section
 * headers.
 */
private class TemplateBackend(private val list: ShadowListUtilsList, private val controller: ListController) :
  ListBackend {
  /*
   * The row each cell showed at the last frame, to tell a cell bound again.
   */
  private val shown = WeakHashMap<View, ShadowListUtilsRow>()

  override val view: View get() = list

  init {
    applyPadding(list)
    if (Settings.drawDelayMs > 0) {
      list.viewTreeObserver.addOnPreDrawListener {
        delayNewlyBound()
        true
      }
    }
  }

  /*
   * The bench's self-test shows every row late. The template binds its own cells, which this
   * finds before each frame by the row they show.
   */
  private fun delayNewlyBound() {
    for (index in 0 until list.childCount) {
      val cell = list.getChildAt(index) as? ShadowListUtilsRowView ?: continue
      val row = cell.row ?: continue
      if (cell.visibility != View.VISIBLE || shown[cell] === row) continue
      shown[cell] = row
      controller.delayDraw(cell)
    }
  }

  override fun setRows(rows: List<ShadowListUtilsRow>, change: RowChange) = list.setRows(rows, Settings.animate)

  override fun setStickyIndices(indices: IntArray) {
    list.stickyIndices = indices
  }

  override fun setFooter(footer: View?) {
    list.footerView = footer
  }

  override fun scrollToItem(index: Int, animated: Boolean) = list.scrollToItem(index, 0.0, animated)
}

// endregion

// region ShadowListKitListView

/*
 * A plain ShadowListKitListView without sizes: it measures every row through its cell, and the
 * row view lays itself out on the UI thread.
 */
private class SelfSizingBackend(
  context: Context,
  private val controller: ListController,
  inverted: Boolean,
  columns: Int,
) : ListBackend, ShadowListKitListView.DataSource {
  private val list = ShadowListKitListView(context)
  private val registered = HashSet<String>()

  override val view: View get() = list

  init {
    list.setBackgroundColor(context.listTheme.colors.background)
    list.inverted = inverted
    list.numberOfColumns = columns
    list.dataSource = this
    list.animatesChanges = Settings.animate
    applyPadding(list)
  }

  override fun numberOfItems(listView: ShadowListKitListView) = controller.rows.size
  override fun keyForItem(listView: ShadowListKitListView, index: Int) = controller.rows[index].key

  /*
   * Cells come from the row's own reuse identifier.
   */
  override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
    val row = controller.rows[index]
    if (registered.add(row.reuseIdentifier)) listView.registerCell(row.reuseIdentifier) { row.makeView(it) }
    val cell = listView.dequeueReusableCell<ShadowListUtilsRowView>(row.reuseIdentifier)
    cell.configure(row, null)
    controller.delayDraw(cell)
    return cell
  }

  override fun setRows(rows: List<ShadowListUtilsRow>, change: RowChange) = when (change) {
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
  private val viewTypeRows = HashMap<Int, ShadowListUtilsRow>()
  private var footer: View? = null

  override val view: View get() = recycler

  init {
    recycler.setBackgroundColor(context.listTheme.colors.background)
    recycler.setHasFixedSize(true)
    recycler.itemAnimator = if (Settings.animate) DefaultItemAnimator() else null
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
      return viewTypes.getOrPut(row.reuseIdentifier) { viewTypes.size }.also { viewTypeRows[it] = row }
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
      holder.itemView.layoutParams.height = layout?.heightPx ?: ViewGroup.LayoutParams.WRAP_CONTENT
      (holder.itemView as ShadowListUtilsRowView).configure(row, layout)
      controller.delayDraw(holder.itemView)
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
  override fun setRows(rows: List<ShadowListUtilsRow>, change: RowChange) {
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
  private var header: ShadowListUtilsRowView? = null

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
  private fun headerFor(index: Int): ShadowListUtilsRowView {
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
