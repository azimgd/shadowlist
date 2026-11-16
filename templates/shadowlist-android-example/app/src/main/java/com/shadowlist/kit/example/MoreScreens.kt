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
import com.shadowlist.utils.ShadowListUtilsContactsList
import com.shadowlist.utils.ShadowListUtilsLayoutCache
import com.shadowlist.utils.ShadowListUtilsList
import com.shadowlist.utils.ShadowListUtilsListFooter
import com.shadowlist.utils.ShadowListUtilsListHeader
import com.shadowlist.utils.ShadowListUtilsNestedRow
import com.shadowlist.utils.ShadowListUtilsReorderGrid
import com.shadowlist.utils.ShadowListUtilsReorderLabels
import com.shadowlist.utils.ShadowListUtilsReorderList
import com.shadowlist.utils.ShadowListUtilsReorderRow
import com.shadowlist.utils.ShadowListUtilsRow
import com.shadowlist.utils.ShadowListUtilsRowView
import com.shadowlist.utils.ShadowListUtilsSnapList
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

  /*
   * The screen's own actions in the More menu, in groups.
   */
  fun headerMenu(): List<List<HeaderAction>> = emptyList()
}

/*
 * A ShadowListKitListView over the library's rows, for the screens that show a kit feature with
 * the kit's own calls. The screen is the list's data: it changes its rows and tells the list
 * what changed. With the sl engine sizes come from the rows' layouts, with sl-auto from measuring
 * the cells.
 */
class RowsList(context: Context, engine: Engine) {
  val list = ShadowListKitListView(context)
  var rows: List<ShadowListUtilsRow> = emptyList()
    private set
  private val layouts = ShadowListUtilsLayoutCache()
  private val registered = HashSet<String>()
  private val sized = !engine.selfSizing

  init {
    list.setBackgroundColor(context.listTheme.colors.background)
    list.dataSource = if (sized) SizedSource() else Source()
  }

  private open inner class Source : ShadowListKitListView.DataSource {
    override fun numberOfItems(listView: ShadowListKitListView) = rows.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int) = rows[index].key

    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
      val row = rows[index]
      if (registered.add(row.reuseIdentifier)) listView.registerCell(row.reuseIdentifier) { row.makeView(it) }
      val cell = listView.dequeueReusableCell<ShadowListUtilsRowView>(row.reuseIdentifier)
      val width = (list.width - list.paddingLeft - list.paddingRight) / list.numberOfColumns
      cell.configure(row, if (sized) layouts.layout(row, width) else null)
      return cell
    }
  }

  private inner class SizedSource : Source(), ShadowListKitListView.Sizing {
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int =
      layouts.layout(rows[index], crossSize).heightPx
  }

  fun setRows(rows: List<ShadowListUtilsRow>) {
    this.rows = rows
    list.reloadData()
  }
}

/*
 * Travellers as the reorder template's rows, for the screens above.
 */
private fun travellerRows(context: Context, count: Int, first: Int = 0): List<ShadowListUtilsRow> {
  val theme = context.listTheme
  val labels = ShadowListUtilsReorderLabels()
  return List(count) { ShadowListUtilsReorderRow(Items.contact(Contact(first + it)), theme, labels) }
}

/*
 * A plain list of travellers. A tap opens the contact; a trailing swipe deletes it.
 */
class ContactsScreen(context: Context) : Screen {
  private val list = ShadowListUtilsContactsList(context)
  override val view: View get() = list

  override fun load() {
    list.onPressItem = { (view.context as? MainActivity)?.openContact(it) }
    list.onDelete = { deleted -> list.setItems(list.items.filter { it.key != deleted.key }, true) }
    list.items = List(Settings.count) { Items.contact(Contact(it)) }
  }
}

/*
 * Touch and hold a traveller, then drag it to change the boarding order.
 */
class ReorderScreen(context: Context) : Screen {
  private val list = ShadowListUtilsReorderList(context)
  override val view: View get() = list

  override fun load() {
    list.headerView = ShadowListUtilsListFooter(
      view.context, "Touch and hold a traveller, then drag to change the order.", view.context.listTheme)
    list.onMoveItem = { from, to, _ -> Log.i("SLEXAMPLE", "moved $from to $to") }
    list.items = List(80) { Items.contact(Contact(it)) }
    AutoDrag.runIfRequested(list, 1) { list.items.take(6).map { it.name } }
  }
}

/*
 * The same in a grid of numbered tiles, which stack like a masonry gallery.
 */
class ReorderGridScreen(context: Context) : Screen {
  private val list = ShadowListUtilsReorderGrid(context)
  override val view: View get() = list

  override fun load() {
    list.onMoveItem = { from, to, _ -> Log.i("SLEXAMPLE", "moved $from to $to") }
    list.tileAspectRatio = 1f
    list.items = List(60) { Items.tile(it) }
    list.headerView = ShadowListUtilsListFooter(view.context,
      "Touch and hold a sight, then drag it anywhere in the grid.", view.context.listTheme)
    AutoDrag.runIfRequested(list, 3) { list.items.take(9).map { it.label ?: it.key } }
  }
}

/*
 * Destination cards a quarter of the screen tall. A fling rests on a card's top edge.
 */
class SnapScreen(context: Context) : Screen {
  private val list = ShadowListUtilsSnapList(context)
  override val view: View get() = list

  override fun load() {
    list.items = List(50) { Items.snap(it) }
    AutoFling.runIfRequested(list)
  }
}

/*
 * Shelves of pictures, each a horizontal list inside a row of a vertical one. A shelf keeps its
 * place while it is off screen.
 */
class NestedScreen(context: Context) : Screen {
  private val list = ShadowListUtilsList(context)
  override val view: View get() = list

  override fun load() {
    val context = view.context
    list.headerView = DealsShelf(context)
    list.setRows(List(40) { shelf ->
      ShadowListUtilsNestedRow(Items.shelf(shelf), context.listTheme) { card, row ->
        Log.i("SLEXAMPLE", "pressed ${card.title} in ${row.title}")
      }
    })
  }
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
    page.setBackgroundColor(context.listTheme.colors.background)
    page.addView(ShadowListUtilsListFooter(context, "Destinations by letter, scroll sideways.", context.listTheme),
      LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
    page.addView(list, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, StripItem.HEIGHT_DP.dp))
    list.horizontal = true
    list.estimatedItemSizePx = 150.dpf
    list.registerCell("strip") { StripCell(it) }
  }

  override fun load() {
    val titles = List(600) { FixtureStrings.imageTitles[it % FixtureStrings.imageTitles.size] to it }
      .sortedBy { it.first }
    val sticky = ArrayList<Int>()
    val palette = view.context.listTheme.colors.avatarPalette
    var letter = ""
    for ((title, index) in titles) {
      val first = title.take(1).uppercase()
      if (first != letter) {
        letter = first
        sticky.add(items.size)
        items.add(StripItem("head-$first", first, true, 0))
      }
      items.add(StripItem("strip-$index", title, false, palette[items.size % palette.size]))
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
      val x = start.centerX() + list.paddingLeft - list.scrollX
      val y = start.centerY() + list.paddingTop - list.scrollY
      val dx = if (columns > 1) start.width() else 0f
      val dy = start.height() * (if (columns > 1) 2.2f else 4.2f)
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
  /*
   * How long the list goes without a scroll event before a fling counts as ended.
   */
  private const val SCROLL_IDLE_MS = 250L

  fun runIfRequested(list: ShadowListUtilsList) {
    if (LaunchArgs.get("SLScenario") != "snap") return
    val script = TouchScript(list)
    val handler = Handler(Looper.getMainLooper())
    var onEndScrolling: (() -> Unit)? = null
    val ended = Runnable { onEndScrolling?.invoke() }
    // The template list owns its delegate. Its scroll events going quiet mark the end of a fling.
    list.onScroll = {
      handler.removeCallbacks(ended)
      handler.postDelayed(ended, SCROLL_IDLE_MS)
    }
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
      onEndScrolling = {
        onEndScrolling = null
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
  private val items = ArrayList<ShadowListUtilsRow>()
  private var fresh = 1000
  override val view: View get() = rows.list

  override fun load() {
    rows.list.animatesChanges = true
    items.addAll(travellerRows(view.context, 80))
    rows.setRows(items)
    if (LaunchArgs.get("SLScenario") == "animate") TouchScript(rows.list).after(1500) { runCheck() }
  }

  override fun headerMenu() = listOf(listOf(HeaderAction("Change Travellers", onPress = ::change)))

  /*
   * Remove the row at 2 and insert two rows at 4, through deleteItems and insertItems.
   */
  fun change() {
    if (items.size <= 4) return
    items.removeAt(2)
    rows.list.deleteItems(intArrayOf(2))
    items.addAll(4, travellerRows(view.context, 2, fresh))
    fresh += 2
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
      val cell = list.getChildAt(index) as? ShadowListUtilsRowView ?: continue
      if (cell.visibility == View.VISIBLE && cell.row?.key == key) ++count
    }
    return count
  }
}

/*
 * The list under a collapsing bar in a CoordinatorLayout, which needs nested scrolling: an
 * elevated space that scrolls away above a caption that stays pinned. With --es SLScenario nested
 * a script drags the list up, then down, and logs how far the bar and the list moved each time.
 */
class CollapsingScreen(context: Context, engine: Engine) : Screen {
  private val themed = android.view.ContextThemeWrapper(context, com.google.android.material.R.style.Theme_MaterialComponents_Light_NoActionBar)
  private val rows = RowsList(themed, engine)
  private val coordinator = androidx.coordinatorlayout.widget.CoordinatorLayout(themed)
  private val bar = com.google.android.material.appbar.AppBarLayout(themed)
  override val view: View get() = coordinator

  init {
    bar.setBackgroundColor(themed.listTheme.colors.background)
    bar.stateListAnimator = null
    bar.elevation = 0f
    val space = View(themed)
    space.setBackgroundColor(themed.listTheme.colors.elevated)
    val params = com.google.android.material.appbar.AppBarLayout.LayoutParams(
      android.view.ViewGroup.LayoutParams.MATCH_PARENT, SPACE_HEIGHT_DP.dp)
    params.scrollFlags = com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL
    bar.addView(space, params)
    // A child without scroll flags stays pinned once the space above it has scrolled away.
    bar.addView(ShadowListUtilsListFooter(themed, "A bar that collapses as the list scrolls.", themed.listTheme),
      com.google.android.material.appbar.AppBarLayout.LayoutParams(
        android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.WRAP_CONTENT))
    coordinator.addView(bar, androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams(
      android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.WRAP_CONTENT))
    val listParams = androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams(
      android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.MATCH_PARENT)
    listParams.behavior = com.google.android.material.appbar.AppBarLayout.ScrollingViewBehavior()
    coordinator.addView(rows.list, listParams)
  }

  override fun load() {
    rows.setRows(travellerRows(view.context, 80))
    if (LaunchArgs.get("SLScenario") == "nested") runCheck()
  }

  private fun runCheck() {
    val list = rows.list
    // Touches go in at the window, the way a finger's do. The list moves under them when the bar collapses.
    val script = TouchScript(list.rootView)
    val result = JSONObject().put("scenario", "nested").put("collapseRange", SPACE_HEIGHT_DP.dp)
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

  private companion object {
    /*
     * With the caption this makes the 200dp bar of the React Native screen.
     */
    const val SPACE_HEIGHT_DP = 142
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
