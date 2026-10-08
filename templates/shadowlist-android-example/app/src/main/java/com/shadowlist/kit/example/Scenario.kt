package com.shadowlist.kit.example

import android.content.Intent
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.os.SystemClock
import android.util.Log
import android.view.View
import android.view.ViewGroup
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.max

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

  private fun report(result: JSONObject, name: String) {
    result.put("scenario", name).put("engine", screen.engine.id).put("screen", screen.javaClass.simpleName)
    Log.i(TAG, result.toString())
    if (exit) after(300) { Process.killProcess(Process.myPid()) }
  }

  /*
   * Window y of every row view on screen, by row key.
   */
  private fun visibleRows(): Map<String, Int> {
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
}
