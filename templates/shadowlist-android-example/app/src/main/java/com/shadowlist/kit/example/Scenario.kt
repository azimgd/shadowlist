package com.shadowlist.kit.example

import android.content.Intent
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.os.SystemClock
import android.util.Log
import android.view.Choreographer
import android.view.View
import android.view.ViewGroup
import com.shadowlist.kit.ShadowListKitListView
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/*
 * Scripted checks, started with --es SLScenario <name> on a list screen. Each logs one
 * SLSCENARIO JSON line and quits with --es SLBenchExit 1. The same scenarios as the UIKit example.
 *
 *   prepend  scroll into the list, add rows at the start, measure how far visible rows moved
 *   append   same with rows added at the end
 *   cost     UI thread time of one data update plus the layout it causes, medians, with the
 *            layout prefetch finished and paused. SLCostRuns updates of each kind, default 15.
 *            prependMs and appendMs count the list's work, the *TotalMs fields add the
 *            screen building its rows
 *   edges    prepend and append at the start, at the end and during a fling, shrink, grow back
 *            and grow from one row. Logs each step's largest row shift and the largest
 *            uncovered stretch of the viewport in any frame
 */
class Scenario private constructor(private val screen: ListScreen, private val exit: Boolean) {
  companion object {
    private const val TAG = "SLSCENARIO"

    fun startIfRequested(screen: ListScreen, intent: Intent) {
      val name = intent.getStringExtra("SLScenario") ?: return
      val scenario = Scenario(screen, intent.getStringExtra("SLBenchExit") == "1")
      scenario.costRuns = intent.getStringExtra("SLCostRuns")?.toIntOrNull()?.takeIf { it > 0 } ?: 15
      when (name) {
        // The background layout prefetch finishes first. It would compete with the timed updates.
        "cost" -> scenario.after(2000) { screen.list.layouts.whenIdle { scenario.measureUpdateCost() } }
        "prepend", "append" -> scenario.after(1500) { scenario.measureShift(name) }
        "edges" -> scenario.after(1500) { EdgeChecks(screen) { scenario.report(it, name) }.start() }
      }
    }
  }

  private val handler = Handler(Looper.getMainLooper())

  // Updates of each kind the cost scenario times, --es SLCostRuns.
  private var costRuns = 15
  private val view: View get() = screen.view

  private fun after(millis: Long, work: () -> Unit) {
    handler.postDelayed(work, millis)
  }

  /*
   * Rest mid list, change the data, and report how far the rows on screen moved.
   */
  private fun measureShift(name: String) {
    view.scrollBy(0, 4000)
    after(1000) {
      val before = visibleRows()
      if (name == "prepend") screen.prependRows() else screen.appendRows()
      after(1000) {
        val after = visibleRows()
        var shift = 0
        var kept = 0
        for ((key, y) in before) {
          val moved = after[key] ?: continue
          shift = max(shift, abs(moved - y))
          ++kept
        }
        report(JSONObject().put("maxShift", shift).put("visibleBefore", before.size).put("stillVisible", kept), name)
      }
    }
  }

  /*
   * Prepends then appends while resting mid list. Each is timed twice: the list's own work,
   * the update call plus a synchronous measure and layout, and the total with the screen
   * building its rows, which is the same for every engine.
   */
  private fun measureUpdateCost() {
    view.scrollBy(0, 20000)
    forceLayout()
    screen.list.layouts.prefetchPaused = true
    val prepends = List(costRuns) { timed { screen.prependRows() } }
    val appends = List(costRuns) { timed { screen.appendRows() } }
    screen.list.layouts.prefetchPaused = false
    fun median(values: List<Double>) = values.sorted()[values.size / 2]
    report(JSONObject().put("rows", screen.list.rows.size)
      .put("prependMs", median(prepends.map { it.first })).put("appendMs", median(appends.map { it.first }))
      .put("prependTotalMs", median(prepends.map { it.second }))
      .put("appendTotalMs", median(appends.map { it.second })), "cost")
  }

  /*
   * The list's time and the total time of one update, in milliseconds.
   */
  private fun timed(work: () -> Unit): Pair<Double, Double> {
    val start = SystemClock.elapsedRealtimeNanos()
    work()
    val layoutStart = SystemClock.elapsedRealtimeNanos()
    forceLayout()
    val end = SystemClock.elapsedRealtimeNanos()
    val list = screen.list.lastUpdateNanos + (end - layoutStart)
    return Pair(list / 1e6, (end - start) / 1e6)
  }

  /*
   * Measure and lay out the list now. RecyclerView with a fixed size otherwise defers adapter
   * updates to the next frame, outside the timed span.
   */
  private fun forceLayout() {
    view.forceLayout()
    view.measure(
      View.MeasureSpec.makeMeasureSpec(view.width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(view.height, View.MeasureSpec.EXACTLY))
    view.layout(view.left, view.top, view.right, view.bottom)
  }

  fun report(result: JSONObject, name: String) {
    result.put("scenario", name).put("engine", screen.engine.id).put("screen", screen.javaClass.simpleName)
    Log.i(TAG, result.toString())
    if (exit) after(300) { Process.killProcess(Process.myPid()) }
  }

  private fun visibleRows(): Map<String, Int> = visibleRowsOf(view)
}

/*
 * Window y of every row view on screen, by row key.
 */
fun visibleRowsOf(view: View): Map<String, Int> {
  val rows = HashMap<String, Int>()
  val location = IntArray(2)
  view.getLocationOnScreen(location)
  val top = location[1]
  val bottom = top + view.height
  val group = view as? ViewGroup ?: return rows
  for (index in 0 until group.childCount) {
    val child = group.getChildAt(index) as? RowView ?: continue
    val key = child.row?.key ?: continue
    if (child.visibility != View.VISIBLE) continue
    child.getLocationOnScreen(location)
    if (location[1] + child.height > top && location[1] < bottom) rows[key] = location[1]
  }
  return rows
}

/*
 * The edges scenario. Steps run one after another. A frame monitor records, for every frame,
 * how far the rows moved since the previous one and the viewport length no row covers where the
 * list has content.
 */
private class EdgeChecks(private val screen: ListScreen, private val done: (JSONObject) -> Unit) {
  private val handler = Handler(Looper.getMainLooper())
  private val view: View get() = screen.view
  private val result = JSONObject()
  private val original = screen.list.rows

  /*
   * --es SLEdgeSteps shrink,growBack runs only those steps.
   */
  private val only = LaunchArgs.get("SLEdgeSteps")?.split(",")
  private var previousFrame: Map<String, Int> = emptyMap()
  private var previousDelta: Int? = null
  private var maxGap = 0
  private var maxJerk = 0
  private var maxRelative = 0
  private var monitoring = true
  private var currentStep = ""

  private val monitor = object : Choreographer.FrameCallback {
    override fun doFrame(frameTimeNanos: Long) {
      sampleFrame()
      if (monitoring) Choreographer.getInstance().postFrameCallback(this)
    }
  }

  fun start() {
    Choreographer.getInstance().postFrameCallback(monitor)
    val last = original.size - 1
    val steps = listOf<Pair<String, (() -> Unit) -> Unit>>(
      "startPrepend" to { next -> shiftAt(0, { screen.prependRows() }, next) },
      "startAppend" to { next -> shiftAt(0, { screen.appendRows() }, next) },
      "endPrepend" to { next -> shiftAt(screen.list.rows.size - 1, { screen.prependRows() }, next) },
      "endAppend" to { next -> shiftAt(screen.list.rows.size - 1, { screen.appendRows() }, next) },
      "flingChanges" to { next -> flingWithChanges(next) },
      "shrink" to { next -> shiftAt(screen.list.rows.size / 2, { screen.list.setRows(screen.list.rows.take(3), RowChange.Update) }, next) },
      "growBack" to { next -> shift({ screen.list.setRows(original, RowChange.Update) }, next) },
      "growFromOne" to { next -> growFromOne(next) },
      "resize" to { next -> resize(next) },
      "commands" to { next -> commands(next) },
      "reached" to { next -> reached(next) },
      "followAppend" to { next -> followAppend(next) },
    ).filter { only == null || it.first in only }
    fun run(index: Int) {
      if (index == steps.size) {
        monitoring = false
        done(result)
        return
      }
      val (name, step) = steps[index]
      resetMonitor()
      step {
        result.put(name + "Gap", maxGap).put(name + "Jerk", maxJerk).put(name + "Relative", maxRelative)
        handler.postDelayed({ run(index + 1) }, 300)
      }
      currentStep = name
    }
    if (last >= 0) run(0) else done(result.put("error", "empty"))
  }

  private fun resetMonitor() {
    maxGap = 0
    maxJerk = 0
    maxRelative = 0
    previousDelta = null
    previousFrame = emptyMap()
  }

  /*
   * Rest on the row, let the scroll settle, change the data and record how far the rows on
   * screen moved.
   */
  private fun shiftAt(index: Int, change: () -> Unit, next: () -> Unit) {
    screen.list.backend.scrollToIndex(index, false)
    handler.postDelayed({ shift(change, next) }, 700)
  }

  private fun shift(change: () -> Unit, next: () -> Unit) {
    resetMonitor()
    val before = visibleRowsOf(view)
    change()
    handler.postDelayed({
      val after = visibleRowsOf(view)
      var moved = 0
      var kept = 0
      for ((key, y) in before) {
        val now = after[key] ?: continue
        moved = max(moved, abs(now - y))
        ++kept
      }
      result.put(currentStep + "Shift", moved).put(currentStep + "Kept", "$kept/${before.size}")
      next()
    }, 700)
  }

  /*
   * Fling toward the end from the middle, prepend and append on the way, and let it come to rest.
   */
  private fun flingWithChanges(next: () -> Unit) {
    screen.list.backend.scrollToIndex(screen.list.rows.size / 2, false)
    handler.postDelayed({
      resetMonitor()
      val script = TouchScript(view)
      val x = view.width / 2f
      val y = view.height * 0.8f
      val step = view.height * 0.1f
      script.down(x, y)
      script.frames(5, { frame -> script.move(x, y - step * (frame + 1)) }) {
        script.up(x, y - step * 5)
        // Synthesized moves don't line up with frames. Judge the momentum only.
        resetMonitor()
        handler.postDelayed({ screen.prependRows() }, 150)
        handler.postDelayed({ screen.appendRows() }, 350)
        handler.postDelayed(next, 2500)
      }
    }, 700)
  }

  /*
   * One row, then one more each 100 ms. Records where the first and last rows sit afterwards.
   */
  private fun growFromOne(next: () -> Unit) {
    screen.list.setRows(original.take(1), RowChange.Update)
    var count = 1
    val grow = object : Runnable {
      override fun run() {
        if (count < 8) {
          ++count
          screen.list.setRows(original.take(count), RowChange.Append(1))
          handler.postDelayed(this, 100)
          return
        }
        handler.postDelayed({
          val rows = visibleRowsOf(view)
          val location = IntArray(2)
          view.getLocationOnScreen(location)
          val group = view as ViewGroup
          var lastBottom = Int.MIN_VALUE
          for (index in 0 until group.childCount) {
            val child = group.getChildAt(index) as? RowView ?: continue
            if (child.visibility != View.VISIBLE || child.row == null) continue
            val at = IntArray(2)
            child.getLocationOnScreen(at)
            lastBottom = max(lastBottom, at[1] + child.height)
          }
          result.put("growVisible", rows.size).put("growFirstTop", (rows[original[0].key] ?: -1) - location[1])
            .put("growLastBottomFromEnd", location[1] + view.height - lastBottom)
          screen.list.setRows(original, RowChange.Update)
          handler.postDelayed(next, 300)
        }, 700)
      }
    }
    handler.postDelayed(grow, 300)
  }

  private val kit: ShadowListKitListView? get() = view as? ShadowListKitListView

  /*
   * Narrow the list mid scroll, like a rotation or a split screen, then widen it back. The row
   * at the top stays at the top.
   */
  private fun resize(next: () -> Unit) {
    screen.list.backend.scrollToIndex(screen.list.rows.size / 2, false)
    handler.postDelayed({
      val top = visibleRowsOf(view).minByOrNull { it.value } ?: return@postDelayed next()
      val width = view.width
      view.layoutParams = view.layoutParams.apply { this.width = width * 2 / 3 }
      handler.postDelayed({
        val narrow = visibleRowsOf(view)[top.key]
        view.layoutParams = view.layoutParams.apply { this.width = ViewGroup.LayoutParams.MATCH_PARENT }
        handler.postDelayed({
          val back = visibleRowsOf(view)[top.key]
          result.put("resizeNarrowShift", narrow?.let { abs(it - top.value) } ?: -1)
            .put("resizeBackShift", back?.let { abs(it - top.value) } ?: -1)
          next()
        }, 700)
      }, 700)
    }, 700)
  }

  /*
   * Animated and immediate scroll commands on ShadowListKitListView. Records how far each landed
   * from where it was asked to.
   */
  private fun commands(next: () -> Unit) {
    val list = kit ?: return next()
    val middle = screen.list.rows.size / 2
    val window = list.height - list.paddingTop - list.paddingBottom
    fun centerError(index: Int): Int {
      val rect = list.rectForItem(index) ?: return Int.MAX_VALUE
      return abs((rect.centerY() - list.scrollY - list.paddingTop - window / 2f).toInt())
    }
    list.scrollToItem(middle, 0.5, animated = true)
    handler.postDelayed({
      result.put("animatedToItemCenterError", centerError(middle))
      list.scrollToEnd(animated = true)
      handler.postDelayed({
        result.put("animatedToEndError", list.contentSize - window - list.scrollY)
        list.scrollToStart(animated = true)
        handler.postDelayed({
          result.put("animatedToStartError", list.scrollY)
          list.scrollToItem(middle + 7, 0.5)
          handler.postDelayed({
            result.put("toItemCenterError", centerError(middle + 7))
            next()
          }, 500)
        }, 2000)
      }, 2000)
    }, 2000)
  }

  /*
   * The reached callbacks fire at each end. A load at the end appends without moving the rows.
   */
  private fun reached(next: () -> Unit) {
    val list = kit ?: return next()
    val previous = list.delegate
    var starts = 0
    var ends = 0
    var loaded = false
    list.delegate = object : ShadowListKitListView.Delegate {
      override fun didReachStart(listView: ShadowListKitListView) { ++starts }
      override fun didReachEnd(listView: ShadowListKitListView) {
        ++ends
        if (!loaded) {
          loaded = true
          handler.post { screen.appendRows() }
        }
      }
    }
    list.scrollToItem(screen.list.rows.size / 2)
    handler.postDelayed({
      starts = 0
      ends = 0
      val rowsBefore = screen.list.rows.size
      list.scrollToEnd()
      handler.postDelayed({
        val before = visibleRowsOf(view)
        handler.postDelayed({
          val after = visibleRowsOf(view)
          result.put("reachedEnds", ends).put("loadedRows", screen.list.rows.size - rowsBefore)
            .put("loadShift", before.entries.maxOfOrNull { (key, y) -> after[key]?.let { abs(it - y) } ?: 0 } ?: 0)
          list.scrollToStart()
          handler.postDelayed({
            result.put("reachedStarts", starts)
            list.delegate = previous
            next()
          }, 500)
        }, 500)
      }, 100)
    }, 500)
  }

  /*
   * With followAppends an inverted list at its end scrolls appended rows into view.
   */
  private fun followAppend(next: () -> Unit) {
    val list = kit ?: return next()
    if (!list.inverted) return next()
    list.followAppends = true
    list.scrollToEnd()
    handler.postDelayed({
      screen.appendRows()
      handler.postDelayed({
        val window = list.height - list.paddingTop - list.paddingBottom
        result.put("followAppendFromEnd", list.contentSize - window - list.scrollY)
        list.followAppends = false
        next()
      }, 700)
    }, 500)
  }

  private fun sampleFrame() {
    val rows = visibleRowsOf(view)
    val deltas = ArrayList<Int>()
    for ((key, y) in rows) previousFrame[key]?.let { deltas.add(y - it) }
    if (deltas.isNotEmpty()) {
      deltas.sort()
      val delta = deltas[deltas.size / 2]
      maxRelative = max(maxRelative, max(abs(deltas.first() - delta), abs(deltas.last() - delta)))
      previousDelta?.let { maxJerk = max(maxJerk, abs(delta - it)) }
      previousDelta = delta
    } else {
      previousDelta = null
    }
    previousFrame = rows
    maxGap = max(maxGap, uncovered())
  }

  /*
   * Viewport length no child covers, between the first and last child and at each edge the
   * list can still scroll past.
   */
  private fun uncovered(): Int {
    val group = view as? ViewGroup ?: return 0
    val spans = ArrayList<IntArray>()
    for (index in 0 until group.childCount) {
      val child = group.getChildAt(index)
      if (child.visibility != View.VISIBLE || child !is RowView && child.javaClass.simpleName != "ListFooterView" &&
        child !is android.widget.FrameLayout) continue
      val top = (child.top + child.translationY).toInt() - view.scrollY
      spans.add(intArrayOf(top, top + child.height))
    }
    if (spans.isEmpty()) return if (screen.list.rows.isEmpty()) 0 else view.height
    spans.sortBy { it[0] }
    val low = if (view.canScrollVertically(-1)) 0 else max(0, spans.first()[0])
    val high = if (view.canScrollVertically(1)) view.height else min(view.height, spans.maxOf { it[1] })
    var covered = low
    var gap = 0
    for (span in spans) {
      if (span[0] > covered) gap += min(span[0], high) - covered
      covered = max(covered, span[1])
      if (covered >= high) break
    }
    if (covered < high) gap += high - covered
    return max(0, gap)
  }
}
