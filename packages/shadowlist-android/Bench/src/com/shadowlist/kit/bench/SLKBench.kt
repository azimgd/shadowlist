package com.shadowlist.kit.bench

import android.app.Activity
import android.content.Intent
import android.os.Debug
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.util.Log
import android.view.Choreographer
import android.view.FrameMetrics
import android.view.View
import android.view.ViewGroup
import android.view.Window
import org.json.JSONObject
import java.io.File
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToLong

/*
 * Drives the biggest vertical scroll view of an activity at a constant speed, one step per
 * frame, and logs one JSON line with frame intervals, janky frames, UI thread, RenderThread
 * and process CPU, memory, and how much of the viewport no row covers. The same metrics as
 * the iOS SLKBench. Extras: SLBench 1, SLBenchSpeed (dp/s), SLBenchSeconds per leg,
 * SLBenchDelay, SLBenchLabel, SLBenchExit 1.
 */
class SLKBench private constructor(private val activity: Activity, intent: Intent) : Choreographer.FrameCallback {
  companion object {
    private const val TAG = "SLBENCH"

    fun startIfRequested(activity: Activity, intent: Intent) {
      if (intent.getStringExtra("SLBench") != "1") return
      val bench = SLKBench(activity, intent)
      Handler(Looper.getMainLooper()).postDelayed({ bench.start() }, (bench.delay * 1000).toLong())
    }

    private fun Intent.double(name: String, fallback: Double) = getStringExtra(name)?.toDoubleOrNull() ?: fallback
  }

  private val speed = intent.double("SLBenchSpeed", 4000.0) * activity.resources.displayMetrics.density
  private val legSeconds = intent.double("SLBenchSeconds", 6.0)
  private val delay = intent.double("SLBenchDelay", 3.0)
  private val label = intent.getStringExtra("SLBenchLabel") ?: ""
  private val exit = intent.getStringExtra("SLBenchExit") == "1"

  private lateinit var target: View
  private var direction = 1
  private var legs = 0
  private var lastFrame = 0L
  private var legStart = 0L
  private var travel = 0.0
  private var remainder = 0.0
  private val intervals = ArrayList<Double>(4096)
  private var nominal = 1000.0 / 60.0
  private var uiCpuStart = 0L
  private var processCpuStart = 0L
  private var renderCpuStart = 0L
  private var peakMB = 0.0
  private var frame = 0
  private val blank = BlankSampler()
  private val frameMetrics = FrameMetricsCollector()

  fun start() {
    val found = findScrollView(activity.window.decorView)
    if (found == null) {
      Log.i(TAG, "{\"error\":\"no scroll view\"}")
      return
    }
    target = found
    direction = if (target.canScrollVertically(1)) 1 else -1
    nominal = 1000.0 / (activity.display?.refreshRate ?: 60f)
    uiCpuStart = Debug.threadCpuTimeNanos()
    processCpuStart = Process.getElapsedCpuTime()
    renderCpuStart = ThreadCpu.renderThreadMillis()
    peakMB = MemorySampler.usedMB()
    frameMetrics.attach(activity.window)
    Choreographer.getInstance().postFrameCallback(this)
  }

  /*
   * The vertically scrollable view with the most travel, wider than half the window.
   */
  private fun findScrollView(root: View): View? {
    var best: View? = null
    var bestRange = 0
    fun visit(view: View) {
      if (view.isShown && view.width > root.width / 2 && (view.canScrollVertically(1) || view.canScrollVertically(-1))) {
        val range = view.width * view.height
        if (best == null || range >= bestRange) {
          best = view
          bestRange = range
        }
      }
      if (view is ViewGroup) for (index in 0 until view.childCount) visit(view.getChildAt(index))
    }
    visit(root)
    return best
  }

  override fun doFrame(frameTimeNanos: Long) {
    if (lastFrame == 0L) {
      lastFrame = frameTimeNanos
      legStart = frameTimeNanos
      Choreographer.getInstance().postFrameCallback(this)
      return
    }
    val interval = (frameTimeNanos - lastFrame) / 1e6
    lastFrame = frameTimeNanos
    intervals.add(interval)
    sample()
    val atEdge = step(interval)
    if (atEdge || (frameTimeNanos - legStart) / 1e9 >= legSeconds) {
      ++legs
      direction = -direction
      legStart = frameTimeNanos
      if (legs >= 2) {
        finish()
        return
      }
    }
    Choreographer.getInstance().postFrameCallback(this)
  }

  private fun sample() {
    ++frame
    if (frame % 3 == 0) blank.sample(target)
    if (frame % 30 == 0) peakMB = max(peakMB, MemorySampler.usedMB())
  }

  /*
   * Scroll one frame's distance. Returns whether the list hit an edge.
   */
  private fun step(intervalMs: Double): Boolean {
    if (!target.canScrollVertically(direction)) return true
    val exact = speed * intervalMs / 1000.0 + remainder
    val pixels = exact.toInt()
    remainder = exact - pixels
    target.scrollBy(0, pixels * direction)
    travel += abs(pixels)
    return false
  }

  private fun finish() {
    frameMetrics.detach(activity.window)
    val uiCpu = (Debug.threadCpuTimeNanos() - uiCpuStart) / 1e6
    val processCpu = (Process.getElapsedCpuTime() - processCpuStart).toDouble()
    val renderCpu = (ThreadCpu.renderThreadMillis() - renderCpuStart).toDouble()
    val result = FrameStats(intervals, nominal).toJson().apply {
      put("label", label)
      put("scrollView", target.javaClass.simpleName)
      put("speed", speed / activity.resources.displayMetrics.density)
      put("travel", travel.roundToLong())
      put("mainCpuMsPerS", uiCpu * 1000 / max(1.0, getDouble("seconds") * 1000))
      put("renderCpuMsPerS", renderCpu * 1000 / max(1.0, getDouble("seconds") * 1000))
      put("processCpuMsPerS", processCpu * 1000 / max(1.0, getDouble("seconds") * 1000))
      put("mainCpuUsPerFrame", if (intervals.isEmpty()) 0.0 else uiCpu * 1000 / intervals.size)
      put("peakMB", peakMB)
      put("pssMB", MemorySampler.pssMB())
      blank.write(this)
      frameMetrics.write(this, nominal)
    }
    val line = result.toString()
    Log.i(TAG, line)
    File(activity.filesDir, "slbench.json").writeText(line)
    if (exit) Handler(Looper.getMainLooper()).postDelayed({ activity.finishAndRemoveTask(); Process.killProcess(Process.myPid()) }, 300)
  }
}

/*
 * Frame interval statistics, the same fields the iOS bench reports.
 */
private class FrameStats(private val intervals: List<Double>, private val nominal: Double) {
  fun toJson(): JSONObject {
    val sorted = intervals.sorted()
    var total = 0.0
    var hitch = 0.0
    var dropped = 0L
    var hitches = 0
    for (interval in intervals) {
      total += interval
      if (interval > nominal * 1.5) {
        hitch += interval - nominal
        dropped += (interval / nominal).roundToLong() - 1
        ++hitches
      }
    }
    fun percentile(p: Double) = if (sorted.isEmpty()) 0.0 else sorted[min(sorted.size - 1, (p * (sorted.size - 1)).toInt())]
    val seconds = total / 1000
    return JSONObject().apply {
      put("hz", (1000 / nominal).roundToLong())
      put("seconds", seconds)
      put("frames", intervals.size)
      put("expectedFrames", (total / nominal).roundToLong())
      put("dropped", dropped)
      put("hitches", hitches)
      put("hitchMsPerS", if (seconds > 0) hitch / seconds else 0.0)
      put("p50", percentile(0.5))
      put("p95", percentile(0.95))
      put("p99", percentile(0.99))
      put("max", percentile(1.0))
    }
  }
}

/*
 * Per frame durations from the window, which see the UI thread and RenderThread work of every
 * frame drawn. A frame over its deadline is janky.
 */
private class FrameMetricsCollector : Window.OnFrameMetricsAvailableListener {
  private val handler = Handler(Looper.getMainLooper())
  private val totals = ArrayList<Double>(4096)
  private var overDeadline = 0
  private val uiDurations = ArrayList<Double>(4096)

  fun attach(window: Window) = window.addOnFrameMetricsAvailableListener(this, handler)

  fun detach(window: Window) = runCatching { window.removeOnFrameMetricsAvailableListener(this) }

  override fun onFrameMetricsAvailable(window: Window, metrics: FrameMetrics, dropCount: Int) {
    val total = metrics.getMetric(FrameMetrics.TOTAL_DURATION)
    totals.add(total / 1e6)
    if (android.os.Build.VERSION.SDK_INT >= 31 && total > metrics.getMetric(FrameMetrics.DEADLINE)) ++overDeadline
    val ui = metrics.getMetric(FrameMetrics.INPUT_HANDLING_DURATION) + metrics.getMetric(FrameMetrics.ANIMATION_DURATION) +
      metrics.getMetric(FrameMetrics.LAYOUT_MEASURE_DURATION) + metrics.getMetric(FrameMetrics.DRAW_DURATION)
    uiDurations.add(ui / 1e6)
  }

  fun write(json: JSONObject, nominal: Double) {
    val sorted = totals.sorted()
    val uiSorted = uiDurations.sorted()
    fun at(list: List<Double>, p: Double) = if (list.isEmpty()) 0.0 else list[min(list.size - 1, (p * (list.size - 1)).toInt())]
    json.put("drawnFrames", totals.size)
    // A frame that missed its deadline. Before Android 12 the deadline is one vsync.
    json.put("jankyFrames", if (android.os.Build.VERSION.SDK_INT >= 31) overDeadline else totals.count { it > nominal })
    json.put("uiMsPerFrame", if (uiDurations.isEmpty()) 0.0 else uiDurations.sum() / uiDurations.size)
    json.put("frameP50", at(sorted, 0.5))
    json.put("frameP95", at(sorted, 0.95))
    json.put("frameP99", at(sorted, 0.99))
    json.put("uiP50", at(uiSorted, 0.5))
    json.put("uiP95", at(uiSorted, 0.95))
    json.put("uiP99", at(uiSorted, 0.99))
  }
}

/*
 * How much of the viewport no row covers. Rows are the visible children of the list or of
 * its one container child, at least 30 percent as wide as the list.
 */
private class BlankSampler {
  private var sum = 0.0
  private var maxBlank = 0.0
  private var samples = 0
  private var frames = 0
  private val spans = ArrayList<Pair<Int, Int>>()

  fun sample(list: View) {
    val group = list as? ViewGroup ?: return
    spans.clear()
    for (index in 0 until group.childCount) {
      val child = group.getChildAt(index)
      if (child.visibility != View.VISIBLE || child.alpha < 0.01f || child.width < list.width * 0.3f) continue
      val top = max(0, (child.top + child.translationY).toInt() - list.scrollY)
      val bottom = min(list.height, (child.bottom + child.translationY).toInt() - list.scrollY)
      if (bottom > top) spans.add(top to bottom)
    }
    spans.sortBy { it.first }
    var covered = 0
    var end = 0
    for ((top, bottom) in spans) {
      if (bottom <= end) continue
      covered += bottom - max(top, end)
      end = bottom
    }
    val value = max(0.0, 1 - covered.toDouble() / list.height)
    sum += value
    maxBlank = max(maxBlank, value)
    ++samples
    if (value > 0.02) ++frames
  }

  fun write(json: JSONObject) {
    json.put("blankAvg", if (samples > 0) sum / samples else 0.0)
    json.put("blankMax", maxBlank)
    json.put("blankFrames", frames)
    json.put("blankSamples", samples)
  }
}

private object MemorySampler {
  fun usedMB(): Double {
    val runtime = Runtime.getRuntime()
    return (runtime.totalMemory() - runtime.freeMemory() + Debug.getNativeHeapAllocatedSize()) / 1048576.0
  }

  fun pssMB(): Double = Debug.getPss() / 1024.0
}

/*
 * CPU time of the RenderThread from /proc, in milliseconds.
 */
private object ThreadCpu {
  private val ticksPerSecond = 100.0

  fun renderThreadMillis(): Long {
    val tasks = File("/proc/self/task").listFiles() ?: return 0
    for (task in tasks) {
      val name = runCatching { File(task, "comm").readText().trim() }.getOrNull() ?: continue
      if (name != "RenderThread") continue
      val stat = runCatching { File(task, "stat").readText() }.getOrNull() ?: return 0
      val fields = stat.substringAfterLast(')').trim().split(' ')
      // utime and stime are fields 14 and 15 of stat, 12 and 13 after the name.
      val ticks = fields[11].toLong() + fields[12].toLong()
      return (ticks * 1000 / ticksPerSecond).toLong()
    }
    return 0
  }
}
