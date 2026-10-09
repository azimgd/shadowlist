package com.shadowlist.kit.example

import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.os.SystemClock
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.widget.LinearLayout
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.min

/*
 * A screen of the example: a view to show and data to load.
 */
interface Screen {
  val view: View
  fun load()
}

/*
 * An ShadowListKitListView over example rows, for the feature screens. With the sl engine sizes come
 * from the rows' precomputed layouts, with sl-auto from measuring the cells. These screens
 * always run on ShadowListKitListView.
 */
class RowsList(context: Context, engine: Engine) : ShadowListKitListView.Delegate {
  val list = ShadowListKitListView(context)
  var rows: List<Row> = emptyList()
    private set
  var onMove: ((from: Int, to: Int) -> Unit)? = null
  var onEndScrolling: (() -> Unit)? = null
  private val layouts = LayoutCache()
  private val registered = HashSet<String>()
  private val sized = !engine.selfSizing

  init {
    list.setBackgroundColor(Theme.background)
    list.delegate = this
    list.dataSource = if (sized) SizedSource() else Source()
  }

  private open inner class Source : ShadowListKitListView.DataSource {
    override fun numberOfItems(listView: ShadowListKitListView) = rows.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int) = rows[index].key

    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
      val row = rows[index]
      if (registered.add(row.viewType)) listView.registerCell(row.viewType) { row.makeView(it) }
      val cell = listView.dequeueReusableCell<RowView>(row.viewType)
      val width = (list.width - list.paddingLeft - list.paddingRight) / list.numberOfColumns
      cell.configure(row, if (sized) layouts.layout(row, width) else null)
      return cell
    }
  }

  private inner class SizedSource : Source(), ShadowListKitListView.Sizing {
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int =
      layouts.layout(rows[index], crossSize).height
  }

  fun setRows(rows: List<Row>) {
    this.rows = rows
    list.reloadData()
  }

  override fun moveItem(listView: ShadowListKitListView, sourceIndex: Int, destinationIndex: Int) {
    onMove?.invoke(sourceIndex, destinationIndex)
  }

  override fun didEndScrolling(listView: ShadowListKitListView) {
    onEndScrolling?.invoke()
  }
}

/*
 * Touch and hold a traveller, then drag it to change the boarding order. With columns > 1 the
 * same in a grid of numbered tiles.
 */
class ReorderScreen(context: Context, engine: Engine, private val columns: Int) : Screen {
  private val rows = RowsList(context, engine)
  private val items = ArrayList<Row>()
  override val view: View get() = rows.list

  override fun load() {
    val list = rows.list
    list.reorderEnabled = true
    list.numberOfColumns = columns
    if (columns == 1) {
      items.addAll(List(80) { ReorderRow(Contact(it)) })
      list.headerView = ListFooterView(view.context, "Touch and hold a traveller, then drag to change the order.")
    } else {
      items.addAll(List(60) { TileRow(it) })
    }
    rows.onMove = { from, to -> items.add(to, items.removeAt(from)) }
    rows.setRows(items)
    AutoDrag.runIfRequested(list, columns) { items.take(if (columns == 1) 6 else 9).map(::label) }
  }

  private fun label(row: Row): String = when (row) {
    is ReorderRow -> row.contact.author.name
    is TileRow -> "${row.number}"
    else -> row.key
  }
}

/*
 * Destination cards a quarter of the screen tall. A fling rests on a card's top edge.
 */
class SnapScreen(context: Context, engine: Engine) : Screen {
  private val rows = RowsList(context, engine)
  override val view: View get() = rows.list

  override fun load() {
    rows.list.snapToItem = true
    val height = context().resources.displayMetrics.heightPixels / 4
    rows.setRows(List(50) { SnapRow(it, height) })
    AutoFling.runIfRequested(rows)
  }

  private fun context() = rows.list.context
}

/*
 * A horizontal strip of destination cards grouped by first letter. The letter of the section
 * in view stays pinned at the start. Card widths follow their titles, and the list starts from
 * an estimate.
 */
class HorizontalScreen(context: Context, private val engine: Engine) : Screen, ShadowListKitListView.DataSource {
  private val list = ShadowListKitListView(context)
  private val items = ArrayList<StripItem>()
  private val page = LinearLayout(context)
  override val view: View get() = page

  init {
    page.orientation = LinearLayout.VERTICAL
    page.setBackgroundColor(Theme.background)
    page.addView(ListFooterView(context, "Destinations by letter, scroll sideways."),
      LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
    page.addView(list, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 140.dp))
    list.horizontal = true
    list.estimatedItemSize = 150.dpf
    list.registerCell("strip") { StripCell(it) }
  }

  override fun load() {
    val titles = List(600) { FixtureStrings.imageTitles[it % FixtureStrings.imageTitles.size] to it }
      .sortedBy { it.first }
    val sticky = ArrayList<Int>()
    var letter = ""
    for ((title, index) in titles) {
      val first = title.take(1).uppercase()
      if (first != letter) {
        letter = first
        sticky.add(items.size)
        items.add(StripItem("head-$first", first, true, 0))
      }
      items.add(StripItem("strip-$index", title, false, Theme.avatarPalette[items.size % Theme.avatarPalette.size]))
    }
    list.dataSource = if (engine.selfSizing) this else SizedStrip()
    list.stickyIndices = sticky.toIntArray()
    list.reloadData()
    HorizontalCheck.runIfRequested(list, items)
  }

  override fun numberOfItems(listView: ShadowListKitListView) = items.size
  override fun keyForItem(listView: ShadowListKitListView, index: Int) = items[index].key

  override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
    listView.dequeueReusableCell<StripCell>("strip").also { it.configure(items[index]) }

  /*
   * Widths from the titles, without measuring a cell.
   */
  private inner class SizedStrip : ShadowListKitListView.DataSource by this@HorizontalScreen, ShadowListKitListView.Sizing {
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = items[index].width()
  }
}

/*
 * Synthesized touches for scripted checks. Events go through the view's dispatchTouchEvent,
 * the path a finger takes. Points are in that view's own coordinates.
 */
class TouchScript(private val view: View) {
  private val handler = Handler(Looper.getMainLooper())
  private var downTime = 0L

  fun down(x: Float, y: Float) {
    downTime = SystemClock.uptimeMillis()
    send(MotionEvent.ACTION_DOWN, x, y)
  }

  fun move(x: Float, y: Float) = send(MotionEvent.ACTION_MOVE, x, y)
  fun up(x: Float, y: Float) = send(MotionEvent.ACTION_UP, x, y)

  private fun send(action: Int, x: Float, y: Float) {
    val event = MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, x, y, 0)
    view.dispatchTouchEvent(event)
    event.recycle()
  }

  /*
   * Run step for each of count frames, about 16 ms apart, then done.
   */
  fun frames(count: Int, step: (Int) -> Unit, done: () -> Unit) {
    var frame = 0
    val tick = object : Runnable {
      override fun run() {
        step(frame++)
        if (frame < count) handler.postDelayed(this, 16) else done()
      }
    }
    handler.postDelayed(tick, 16)
  }

  fun after(millis: Long, work: () -> Unit) {
    handler.postDelayed(work, millis)
  }
}

private fun report(tag: String, result: JSONObject, exit: Boolean) {
  Log.i(tag, result.toString())
  if (exit) Handler(Looper.getMainLooper()).postDelayed({ Process.killProcess(Process.myPid()) }, 300)
}

/*
 * --es SLAutoDrag 1 on a reorder screen: touch and hold the second row, carry it four rows
 * down, or in a grid one column right and two rows down, drop it, and log the order before
 * and after. The same check as the UIKit -SLAutoDrag.
 */
object AutoDrag {
  fun runIfRequested(list: ShadowListKitListView, columns: Int, names: () -> List<String>) {
    if (LaunchArgs.get("SLAutoDrag") != "1") return
    val script = TouchScript(list)
    script.after(1500) {
      val before = names()
      val start = list.rectForItem(1) ?: return@after
      val x = start.centerX() - list.scrollX
      val y = start.centerY() - list.scrollY
      val dx = if (columns > 1) start.width() else 0f
      val dy = if (columns > 1) start.height() * 2.2f else 67.dpf * 4.2f
      script.down(x, y)
      script.after(ViewConfiguration.getLongPressTimeout() + 150L) {
        script.frames(30, { frame ->
          val progress = (frame + 1) / 30f
          script.move(x + dx * progress, y + dy * progress)
        }) {
          script.up(x + dx, y + dy)
          script.after(600) {
            report("SLAUTODRAG", JSONObject().put("before", JSONArray(before)).put("after", JSONArray(names()))
              .put("moved", before != names()), LaunchArgs.get("SLBenchExit") == "1")
          }
        }
      }
    }
  }
}

/*
 * --es SLScenario snap on the snap screen: three flings, down, down and up. Each must come to
 * rest with a card's top edge on the viewport's top. Logs where each rested and how far that
 * was from the nearest card edge.
 */
object AutoFling {
  fun runIfRequested(rows: RowsList) {
    if (LaunchArgs.get("SLScenario") != "snap") return
    val list = rows.list
    val script = TouchScript(list)
    val rests = JSONArray()
    var worst = 0f
    val directions = listOf(1, 1, -1)
    fun fling(round: Int) {
      if (round == directions.size) {
        report("SLSCENARIO", JSONObject().put("scenario", "snap").put("rests", rests).put("maxEdgeDistance", worst),
          LaunchArgs.get("SLBenchExit") == "1")
        return
      }
      val direction = directions[round]
      val x = list.width / 2f
      val y = if (direction > 0) list.height * 0.75f else list.height * 0.25f
      rows.onEndScrolling = {
        rows.onEndScrolling = null
        // The pass after the fling ends places the last rows. Judge the rest a frame later.
        script.after(100) {
          val distance = edgeDistance(list)
          worst = maxOf(worst, distance)
          rests.put(JSONObject().put("offset", list.scrollY).put("edgeDistance", distance))
          script.after(300) { fling(round + 1) }
        }
      }
      script.down(x, y)
      script.frames(6, { frame -> script.move(x, y - direction * 70.dpf * (frame + 1)) }) {
        script.up(x, y - direction * 70.dpf * 6)
      }
    }
    script.after(1500) { fling(0) }
  }

  /*
   * Distance from the scroll offset to the nearest row's leading edge.
   */
  private fun edgeDistance(list: ShadowListKitListView): Float {
    val range = list.visibleRange ?: return Float.MAX_VALUE
    var best = Float.MAX_VALUE
    for (index in range) {
      val rect = list.rectForItem(index) ?: continue
      best = min(best, abs(rect.top - list.scrollY))
    }
    return best
  }
}

/*
 * --es SLScenario horizontal on the horizontal screen: step through the strip half a
 * viewport at a time. At each step no gap may show between cells, and a section letter must
 * cover the start of the viewport. Then jump to a far card and check where it lands.
 */
object HorizontalCheck {
  fun runIfRequested(list: ShadowListKitListView, items: List<StripItem>) {
    if (LaunchArgs.get("SLScenario") != "horizontal") return
    val script = TouchScript(list)
    script.after(1500) {
      var gaps = 0
      var uncovered = 0
      var steps = 0
      var offset = 0
      while (true) {
        list.scrollTo(offset, 0)
        val cells = list.visibleCells
        gaps += countGaps(list, cells)
        if (!pinnedHeaderCovers(list, cells)) ++uncovered
        ++steps
        if (offset >= list.contentSize - list.width) break
        offset += list.width / 2
      }
      val expected = items.sumOf { it.width() }
      val target = items.size * 2 / 3
      list.scrollToItem(target, 0.0)
      script.after(300) {
        val landed = list.rectForItem(target)?.left?.minus(list.scrollX) ?: Float.NaN
        report("SLSCENARIO", JSONObject().put("scenario", "horizontal").put("steps", steps).put("gaps", gaps)
          .put("stickyUncovered", uncovered).put("contentWidth", list.contentSize)
          .put("sumOfWidths", expected).put("jumpLanded", landed), LaunchArgs.get("SLBenchExit") == "1")
      }
    }
  }

  /*
   * Gaps between visible cells that are not pinned, in content order.
   */
  private fun countGaps(list: ShadowListKitListView, cells: List<ShadowListKitListCell>): Int {
    val laid = cells.filter { !(it as StripCell).isPinnedHeader(list) }.sortedBy { it.left }
    var gaps = 0
    for (index in 1 until laid.size) if (laid[index].left > laid[index - 1].right) ++gaps
    val first = laid.firstOrNull()
    val last = laid.lastOrNull()
    if (first == null || last == null) return 1
    if (last.right < min(list.scrollX + list.width, list.contentSize)) ++gaps
    return gaps
  }

  private fun pinnedHeaderCovers(list: ShadowListKitListView, cells: List<ShadowListKitListCell>): Boolean =
    cells.any { (it as StripCell).key?.startsWith("head-") == true && it.left <= list.scrollX && it.right > list.scrollX }

  private fun StripCell.isPinnedHeader(list: ShadowListKitListView): Boolean =
    key?.startsWith("head-") == true && left <= list.scrollX
}

/*
 * The launch extras of the running activity, for the scripted checks.
 */
object LaunchArgs {
  private var intent: Intent? = null

  fun set(intent: Intent) {
    this.intent = intent
  }

  fun get(name: String): String? = intent?.getStringExtra(name)
}

/*
 * Travellers that come and go with animatesChanges on. With --es SLScenario animate a script
 * removes the third row and adds two at the fifth, then logs the animation's state while it
 * runs and after it ended.
 */
class ChangesScreen(context: Context, engine: Engine) : Screen {
  private val rows = RowsList(context, engine)
  private val items = ArrayList<Row>()
  private var fresh = 1000
  override val view: View get() = rows.list

  override fun load() {
    rows.list.animatesChanges = true
    items.addAll(List(80) { ReorderRow(Contact(it)) })
    rows.setRows(items)
    if (LaunchArgs.get("SLScenario") == "animate") TouchScript(rows.list).after(1500) { runCheck() }
  }

  /*
   * Remove the row at 2 and insert two rows at 4, through deleteItems and insertItems.
   */
  fun change() {
    items.removeAt(2)
    rows.list.deleteItems(intArrayOf(2))
    items.add(4, ReorderRow(Contact(fresh++)))
    items.add(5, ReorderRow(Contact(fresh++)))
    rows.list.insertItems(intArrayOf(4, 5))
  }

  private fun runCheck() {
    val list = rows.list
    val script = TouchScript(list)
    val topBefore = list.cellForItem(0)?.let { it.top - list.scrollY } ?: -1
    val removedKey = items[2].key
    change()
    list.measure(View.MeasureSpec.makeMeasureSpec(list.width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(list.height, View.MeasureSpec.EXACTLY))
    list.layout(list.left, list.top, list.right, list.bottom)
    script.after(60) {
      val during = JSONObject()
        .put("insertedAlpha", list.cellForItem(4)?.alpha ?: -1f)
        .put("survivorTranslationY", list.cellForItem(7)?.translationY ?: 0f)
        .put("fadingCells", fadingCells(list, removedKey))
      script.after(700) {
        val settled = list.visibleCells.all { it.alpha == 1f && it.translationY == 0f }
        val topAfter = list.cellForItem(0)?.let { it.top - list.scrollY } ?: -1
        report("SLSCENARIO", JSONObject().put("scenario", "animate").put("during", during).put("settled", settled)
          .put("firstRowMoved", topAfter - topBefore).put("fadingAfter", fadingCells(list, removedKey)),
          LaunchArgs.get("SLBenchExit") == "1")
      }
    }
  }

  /*
   * Visible cells that still show the removed row while it fades out.
   */
  private fun fadingCells(list: ShadowListKitListView, key: String): Int {
    var count = 0
    for (index in 0 until list.childCount) {
      val cell = list.getChildAt(index) as? RowView ?: continue
      if (cell.visibility == View.VISIBLE && cell.row?.key == key) ++count
    }
    return count
  }
}

/*
 * The list under a collapsing bar in a CoordinatorLayout, which needs nested scrolling. With
 * --es SLScenario nested a script drags the list up, then down, and logs how far the bar
 * and the list moved each time.
 */
class CollapsingScreen(context: Context, engine: Engine) : Screen {
  private val themed = android.view.ContextThemeWrapper(context, com.google.android.material.R.style.Theme_MaterialComponents_Light_NoActionBar)
  private val rows = RowsList(themed, engine)
  private val coordinator = androidx.coordinatorlayout.widget.CoordinatorLayout(themed)
  private val bar = com.google.android.material.appbar.AppBarLayout(themed)
  override val view: View get() = coordinator

  init {
    // The footer view sizes itself. A frame around it holds the bar's height.
    val title = android.widget.FrameLayout(themed)
    title.setBackgroundColor(Theme.elevated)
    title.minimumHeight = 56.dp
    title.addView(ListFooterView(themed, "A bar that collapses as the list scrolls."), android.widget.FrameLayout.LayoutParams(
      android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.WRAP_CONTENT, android.view.Gravity.BOTTOM))
    val params = com.google.android.material.appbar.AppBarLayout.LayoutParams(
      android.view.ViewGroup.LayoutParams.MATCH_PARENT, 200.dp)
    params.scrollFlags = com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL or
      com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
    bar.addView(title, params)
    coordinator.addView(bar, androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams(
      android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.WRAP_CONTENT))
    val listParams = androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams(
      android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.MATCH_PARENT)
    listParams.behavior = com.google.android.material.appbar.AppBarLayout.ScrollingViewBehavior()
    coordinator.addView(rows.list, listParams)
  }

  override fun load() {
    rows.setRows(List(80) { ReorderRow(Contact(it)) })
    if (LaunchArgs.get("SLScenario") == "nested") runCheck()
  }

  private fun runCheck() {
    val list = rows.list
    // Touches go in at the window, the way a finger's do. The list moves under them when the bar collapses.
    val script = TouchScript(list.rootView)
    val result = JSONObject().put("scenario", "nested").put("collapseRange", 200.dp - 56.dp)
    fun drag(distance: Float, done: () -> Unit) {
      val location = IntArray(2)
      list.getLocationInWindow(location)
      val x = location[0] + list.width / 2f
      val y = location[1] + if (distance < 0) list.height * 0.6f else list.height * 0.2f
      script.down(x, y)
      // Slow enough for no fling: the moves alone must collapse and expand the bar.
      script.frames(40, { frame -> script.move(x, y + distance * (frame + 1) / 40) }) {
        script.after(200) {
          script.up(x, y + distance)
          script.after(400) { done() }
        }
      }
    }
    script.after(1500) {
      drag(-500.dpf) {
        result.put("barTopAfterUp", bar.top).put("listOffsetAfterUp", list.scrollY)
        drag(500.dpf) {
          result.put("barTopAfterDown", bar.top).put("listOffsetAfterDown", list.scrollY)
          report("SLSCENARIO", result, LaunchArgs.get("SLBenchExit") == "1")
        }
      }
    }
  }
}

/*
 * --es SLScenario a11y on a list screen: what the list tells accessibility services, and
 * whether their scroll actions reach rows that are not mounted.
 */
object AccessibilityCheck {
  fun runIfRequested(list: ShadowListKitListView) {
    if (LaunchArgs.get("SLScenario") != "a11y") return
    val script = TouchScript(list)
    script.after(1500) {
      val node = list.createAccessibilityNodeInfo()
      val actions = node.actionList.map { it.id }
      val result = JSONObject().put("scenario", "a11y")
        .put("className", node.className)
        .put("rowCount", node.collectionInfo?.rowCount ?: -1)
        .put("columnCount", node.collectionInfo?.columnCount ?: -1)
        .put("scrollable", node.isScrollable)
        .put("canScrollForward", android.view.accessibility.AccessibilityNodeInfo.ACTION_SCROLL_FORWARD in actions)
        .put("canScrollToPosition", android.R.id.accessibilityActionScrollToPosition in actions)
      val before = list.scrollY
      list.performAccessibilityAction(android.view.accessibility.AccessibilityNodeInfo.ACTION_SCROLL_FORWARD, null)
      result.put("pageScrolledBy", list.scrollY - before)
      val target = 500
      val arguments = android.os.Bundle().apply {
        putInt(android.view.accessibility.AccessibilityNodeInfo.ACTION_ARGUMENT_ROW_INT, target)
        putInt(android.view.accessibility.AccessibilityNodeInfo.ACTION_ARGUMENT_COLUMN_INT, 0)
      }
      list.performAccessibilityAction(android.R.id.accessibilityActionScrollToPosition, arguments)
      script.after(300) {
        val cell = list.cellForItem(target)
        val item = cell?.createAccessibilityNodeInfo()?.collectionItemInfo
        result.put("positionVisible", list.visibleRange?.contains(target) == true)
          .put("cellRowIndex", item?.rowIndex ?: -1)
        report("SLSCENARIO", result, LaunchArgs.get("SLBenchExit") == "1")
      }
    }
  }
}
