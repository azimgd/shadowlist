package com.shadowlist.kit.bench

import android.app.Activity
import android.content.Intent
import android.os.Build
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
 * A view that draws its content after its rows show, like tiles drawn in the background, tells
 * the bench how much of its viewport is not drawn yet, from 0 to 1. The bench asks the driven
 * views and their parents. Results then carry contentBlankAvg, contentBlankMax and
 * contentBlankFrames, where a frame's blank is the larger of the uncovered share and this one.
 */
interface ShadowListKitBenchProbe {
  fun benchBlankFraction(): Double
}

/*
 * Drives the biggest scroll view of an activity at a constant speed, one step per frame, and
 * logs one JSON line per axis with frame intervals, janky frames, UI thread, RenderThread and
 * process CPU, memory, and how much of the viewport no row covers. The same metrics as the iOS
 * ShadowListKitBench. Extras: SLBench 1, SLBenchAxes (y, x or xy, comma separated, default y),
 * SLBenchSpeed (dp/s), SLBenchSeconds per leg, SLBenchDelay, SLBenchLabel, SLBenchExit 1.
 * An axis without a scroll view logs a line with skipped set.
 */
class ShadowListKitBench private constructor(private val activity: Activity, intent: Intent) : Choreographer.FrameCallback {
  companion object {
    private const val TAG = "SLBENCH"

    /*
     * Pause between two axis runs, which lets the last run's work settle.
     */
    private const val RUN_GAP_MS = 500L

    fun startIfRequested(activity: Activity, intent: Intent) {
      if (intent.getStringExtra("SLBench") != "1") return
      val bench = ShadowListKitBench(activity, intent)
      Handler(Looper.getMainLooper()).postDelayed({ bench.start() }, (bench.delay * 1000).toLong())
    }

    private fun Intent.double(name: String, fallback: Double) = getStringExtra(name)?.toDoubleOrNull() ?: fallback
  }

  private val speed = intent.double("SLBenchSpeed", 4000.0) * activity.resources.displayMetrics.density
  private val legSeconds = intent.double("SLBenchSeconds", 6.0)
  private val delay = intent.double("SLBenchDelay", 3.0)
  private val label = intent.getStringExtra("SLBenchLabel") ?: ""
  private val exit = intent.getStringExtra("SLBenchExit") == "1"
  private val axes = (intent.getStringExtra("SLBenchAxes") ?: "y").split(',').map { it.trim() }
    .filter { it == "y" || it == "x" || it == "xy" }.ifEmpty { listOf("y") }

  private var run = 0
  private var axis = "y"
  private var vertical: View? = null
  private var horizontal: View? = null
  private var coverage: View? = null
  private var probe: ShadowListKitBenchProbe? = null
  private var directionX = 1
  private var directionY = 1
  private var legs = 0
  private var lastFrame = 0L
  private var legStart = 0L
  private var travel = 0.0
  private var remainderX = 0.0
  private var remainderY = 0.0
  private var intervals = ArrayList<Double>(4096)
  private var nominal = 1000.0 / 60.0
  private var uiCpuStart = 0L
  private var processCpuStart = 0L
  private var renderCpuStart = 0L
  private var peakMB = 0.0
  private var frame = 0
  private var blank = BlankSampler()
  private var frameMetrics = FrameMetricsCollector()

  fun start() {
    nominal = 1000.0 / refreshRate()
    startRun()
  }

  /*
   * Activity.getDisplay needs Android 11. Older versions read the window manager's display.
   */
  @Suppress("DEPRECATION")
  private fun refreshRate(): Float {
    val display = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) activity.display else activity.windowManager.defaultDisplay
    return display?.refreshRate?.takeIf { it > 0f } ?: 60f
  }

  private fun startRun() {
    axis = axes[run]
    val root = activity.window.decorView
    vertical = if ('y' in axis) findScrollView(root, alongX = false) else null
    horizontal = if ('x' in axis) findScrollView(root, alongX = true) else null
    if (('y' in axis && vertical == null) || ('x' in axis && horizontal == null)) {
      emit(JSONObject().apply {
        put("label", label)
        put("axis", axis)
        put("speed", speed / activity.resources.displayMetrics.density)
        put("skipped", if ('y' in axis && vertical == null) "no vertical scroll view" else "no horizontal scroll view")
      })
      nextRun()
      return
    }
    coverage = vertical ?: findScrollView(root, alongX = false) ?: horizontal
    probe = listOfNotNull(vertical, horizontal).firstNotNullOfOrNull { findProbe(it) }
    directionY = vertical?.let { if (it.canScrollVertically(1)) 1 else -1 } ?: 1
    directionX = horizontal?.let { if (it.canScrollHorizontally(1)) 1 else -1 } ?: 1
    legs = 0
    lastFrame = 0L
    travel = 0.0
    remainderX = 0.0
    remainderY = 0.0
    frame = 0
    intervals = ArrayList(4096)
    blank = BlankSampler()
    frameMetrics = FrameMetricsCollector()
    uiCpuStart = Debug.threadCpuTimeNanos()
    processCpuStart = Process.getElapsedCpuTime()
    renderCpuStart = ThreadCpu.renderThreadMillis()
    peakMB = MemorySampler.usedMB()
    frameMetrics.attach(activity.window)
    Choreographer.getInstance().postFrameCallback(this)
  }

  /*
   * The view scrollable along the axis with the most area, wider than half the window.
   */
  private fun findScrollView(root: View, alongX: Boolean): View? {
    var best: View? = null
    var bestRange = 0
    fun visit(view: View) {
      val scrolls = if (alongX) view.canScrollHorizontally(1) || view.canScrollHorizontally(-1)
      else view.canScrollVertically(1) || view.canScrollVertically(-1)
      if (view.isShown && view.width > root.width / 2 && scrolls) {
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

  /*
   * The view or the first of its parents that answers the probe.
   */
  private fun findProbe(start: View): ShadowListKitBenchProbe? {
    var view: Any? = start
    while (view != null) {
      if (view is ShadowListKitBenchProbe) return view
      view = (view as? View)?.parent
    }
    return null
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
    // A leg ends when its time is up or every driven axis stopped at an edge.
    var atEdge = true
    vertical?.let { atEdge = stepY(it, interval) && atEdge }
    horizontal?.let { atEdge = stepX(it, interval) && atEdge }
    if (atEdge || (frameTimeNanos - legStart) / 1e9 >= legSeconds) {
      ++legs
      directionX = -directionX
      directionY = -directionY
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
    coverage?.let { if (frame % 3 == 0) blank.sample(it, alongX = it === horizontal && it !== vertical, probe) }
    if (frame % 30 == 0) peakMB = max(peakMB, MemorySampler.usedMB())
  }

  /*
   * Scroll one frame's distance down or up. Returns whether the view hit an edge.
   */
  private fun stepY(view: View, intervalMs: Double): Boolean {
    if (!view.canScrollVertically(directionY)) return true
    val exact = speed * intervalMs / 1000.0 + remainderY
    val pixels = exact.toInt()
    remainderY = exact - pixels
    view.scrollBy(0, pixels * directionY)
    travel += abs(pixels)
    return false
  }

  /*
   * Scroll one frame's distance sideways. Returns whether the view hit an edge.
   */
  private fun stepX(view: View, intervalMs: Double): Boolean {
    if (!view.canScrollHorizontally(directionX)) return true
    val exact = speed * intervalMs / 1000.0 + remainderX
    val pixels = exact.toInt()
    remainderX = exact - pixels
    view.scrollBy(pixels * directionX, 0)
    travel += abs(pixels)
    return false
  }

  private fun finish() {
    frameMetrics.detach(activity.window)
    val uiCpu = (Debug.threadCpuTimeNanos() - uiCpuStart) / 1e6
    val processCpu = (Process.getElapsedCpuTime() - processCpuStart).toDouble()
    val renderCpu = (ThreadCpu.renderThreadMillis() - renderCpuStart).toDouble()
    val primary = vertical ?: horizontal
    val result = FrameStats(intervals, nominal).toJson().apply {
      put("label", label)
      put("axis", axis)
      put("scrollView", primary?.javaClass?.simpleName ?: "")
      val separate = horizontal
      if (vertical != null && separate != null && separate !== vertical) put("horizontalScrollView", separate.javaClass.simpleName)
      put("speed", speed / activity.resources.displayMetrics.density)
      put("travel", travel.roundToLong())
      put("mainCpuMsPerS", uiCpu * 1000 / max(1.0, getDouble("seconds") * 1000))
      put("renderCpuMsPerS", renderCpu * 1000 / max(1.0, getDouble("seconds") * 1000))
      put("processCpuMsPerS", processCpu * 1000 / max(1.0, getDouble("seconds") * 1000))
      put("mainCpuUsPerFrame", if (intervals.isEmpty()) 0.0 else uiCpu * 1000 / intervals.size)
      put("peakMB", peakMB)
      put("pssMB", MemorySampler.pssMB())
      probe?.let { put("probe", it.javaClass.simpleName) }
      blank.write(this)
      frameMetrics.write(this, nominal)
    }
    emit(result)
    nextRun()
  }

  /*
   * Log one result line. The file keeps the last one.
   */
  private fun emit(result: JSONObject) {
    val line = result.toString()
    Log.i(TAG, line)
    File(activity.filesDir, "slbench.json").writeText(line)
  }

  private fun nextRun() {
    ++run
    val handler = Handler(Looper.getMainLooper())
    if (run < axes.size) {
      handler.postDelayed({ startRun() }, RUN_GAP_MS)
      return
    }
    if (exit) handler.postDelayed({ activity.finishAndRemoveTask(); Process.killProcess(Process.myPid()) }, 300)
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
    if (Build.VERSION.SDK_INT >= 31 && total > metrics.getMetric(FrameMetrics.DEADLINE)) ++overDeadline
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
    json.put("jankyFrames", if (Build.VERSION.SDK_INT >= 31) overDeadline else totals.count { it > nominal })
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
 * How much of the viewport no row covers, along the list's scroll axis. Rows are the visible
 * children of the list or of its one container child, at least 30 percent of the list's cross
 * size. With a probe, the content blank is the larger of that and the probe's share.
 */
private class BlankSampler {
  private companion object {
    /*
     * A frame counts as blank past this share of the viewport.
     */
    const val BLANK_FRAME = 0.02
  }

  private var sum = 0.0
  private var maxBlank = 0.0
  private var samples = 0
  private var frames = 0
  private var contentSum = 0.0
  private var contentMax = 0.0
  private var contentFrames = 0
  private var probed = false
  private val spans = ArrayList<Pair<Int, Int>>()

  fun sample(list: View, alongX: Boolean, probe: ShadowListKitBenchProbe?) {
    val group = list as? ViewGroup ?: return
    val along = if (alongX) list.width else list.height
    val cross = if (alongX) list.height else list.width
    val scroll = if (alongX) list.scrollX else list.scrollY
    spans.clear()
    for (index in 0 until group.childCount) {
      val child = group.getChildAt(index)
      val childCross = if (alongX) child.height else child.width
      if (child.visibility != View.VISIBLE || child.alpha < 0.01f || childCross < cross * 0.3f) continue
      val start = if (alongX) child.left + child.translationX else child.top + child.translationY
      val end = if (alongX) child.right + child.translationX else child.bottom + child.translationY
      val low = max(0, start.toInt() - scroll)
      val high = min(along, end.toInt() - scroll)
      if (high > low) spans.add(low to high)
    }
    spans.sortBy { it.first }
    var covered = 0
    var end = 0
    for ((low, high) in spans) {
      if (high <= end) continue
      covered += high - max(low, end)
      end = high
    }
    val value = max(0.0, 1 - covered.toDouble() / along)
    sum += value
    maxBlank = max(maxBlank, value)
    ++samples
    if (value > BLANK_FRAME) ++frames
    if (probe != null) {
      probed = true
      val content = max(value, probe.benchBlankFraction().coerceIn(0.0, 1.0))
      contentSum += content
      contentMax = max(contentMax, content)
      if (content > BLANK_FRAME) ++contentFrames
    }
  }

  fun write(json: JSONObject) {
    json.put("blankAvg", if (samples > 0) sum / samples else 0.0)
    json.put("blankMax", maxBlank)
    json.put("blankFrames", frames)
    json.put("blankSamples", samples)
    if (probed) {
      json.put("contentBlankAvg", if (samples > 0) contentSum / samples else 0.0)
      json.put("contentBlankMax", contentMax)
      json.put("contentBlankFrames", contentFrames)
    }
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
  private const val TICKS_PER_SECOND = 100.0

  fun renderThreadMillis(): Long {
    val tasks = File("/proc/self/task").listFiles() ?: return 0
    for (task in tasks) {
      val name = runCatching { File(task, "comm").readText().trim() }.getOrNull() ?: continue
      if (name != "RenderThread") continue
      val stat = runCatching { File(task, "stat").readText() }.getOrNull() ?: return 0
      val fields = stat.substringAfterLast(')').trim().split(' ')
      // utime and stime are fields 14 and 15 of stat, 12 and 13 after the name.
      val ticks = (fields.getOrNull(11)?.toLongOrNull() ?: return 0) + (fields.getOrNull(12)?.toLongOrNull() ?: return 0)
      return (ticks * 1000 / TICKS_PER_SECOND).toLong()
    }
    return 0
  }
}
