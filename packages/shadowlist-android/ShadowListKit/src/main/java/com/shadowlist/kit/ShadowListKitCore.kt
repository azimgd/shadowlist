package com.shadowlist.kit

import androidx.annotation.Keep
import kotlin.math.max

/*
 * The shadowlist core behind one list, through the core's ListDriver in C++. Every call runs
 * on the UI thread. Pass values go in and out through arrays this object reuses.
 */
internal class ShadowListKitCore(private val measure: (row: Int, crossSize: Double) -> Double) {
  companion object {
    init {
      System.loadLibrary("shadowlistkit")
    }

    /*
     * Frees the peer of a core whose list was dropped without destroy(), like one never attached.
     */
    private val reclaimer = ShadowListKitPeerReclaimer(::nativeDestroy)

    const val PASS_OFFSET = 0
    const val PASS_WINDOW_ALONG = 1
    const val PASS_WINDOW_CROSS = 2
    const val PASS_HEADER_SIZE = 3
    const val PASS_FOOTER_SIZE = 4
    const val PASS_PHASE = 5
    const val PASS_USER_SCROLLED = 6
    const val PASS_TRACKING = 7
    const val PASS_OUT_OFFSET = 8
    const val PASS_OUT_CONTENT = 9
    const val PASS_OUT_BAND_LOW = 10
    const val PASS_OUT_BAND_HIGH = 11
    const val PASS_OUT_SETTLING = 12
    const val PASS_OUT_REACHED_START = 13
    const val PASS_OUT_REACHED_END = 14
    const val PASS_OUT_GEOMETRY = 15
    const val PASS_OUT_MEASURED_LOW = 16
    const val PASS_OUT_MEASURED_HIGH = 17
    const val PASS_OUT_FOOTER_START = 18
    const val PASS_SLOTS = 19

    /*
     * Values of PASS_PHASE, the same as the core's ScrollPhase.
     */
    const val PHASE_IDLE = 0
    const val PHASE_DRAGGING = 1
    const val PHASE_SETTLING = 2

    /*
     * Where an animated scroll command lands, the same as the core's ScrollLanding::Target.
     */
    const val LANDING_ROW = 1
    const val LANDING_START = 2
    const val LANDING_END = 3

    /*
     * Bits of a section's flags in setSections.
     */
    const val SECTION_HEADER = 1
    const val SECTION_FOOTER = 2

    /*
     * Kinds of a row in placeOfRow, the same order as the core's RowKind, then the low bits they take.
     */
    const val ROW_ITEM = 0
    const val ROW_HEADER = 1
    const val ROW_FOOTER = 2
    const val ROW_KIND_BITS = 2
    const val ROW_KIND_MASK = 3

    /*
     * Slots at the start of a dragFrame out array, the same as DragFrameSlot in the JNI. Each
     * row's shift follows as an along, cross pair.
     */
    const val DRAG_FRAME_HELD_ALONG = 0
    const val DRAG_FRAME_HELD_CROSS = 1
    const val DRAG_FRAME_INSERTION_MOVED = 2
    const val DRAG_FRAME_SLOTS = 3

    /*
     * Slots of constants, the same as ConstantSlot in the JNI.
     */
    private const val CONSTANT_SWIPE_FLING_VELOCITY = 0
    private const val CONSTANT_SWIPE_SETTLE_DURATION_MS = 1
    private const val CONSTANT_DRAG_LIFT_SCALE = 2
    private const val CONSTANT_DRAG_LIFT_DURATION_MS = 3
    private const val CONSTANT_DRAG_SHIFT_DURATION_MS = 4
    private const val CONSTANT_DRAG_DROP_DURATION_MS = 5
    private const val CONSTANT_SECTION_INDEX_TITLE_HEIGHT = 6
    private const val CONSTANT_SECTION_INDEX_WIDTH = 7
    private const val CONSTANT_CHANGE_DURATION_MS = 8
    private const val CONSTANT_OFFSET_MOVED_THRESHOLD = 9

    private val constants: DoubleArray by lazy { nativeConstants() }

    /*
     * The core's swipe constants: the fling speed in dp per second and the settle duration.
     */
    val SWIPE_FLING_VELOCITY_DP: Double get() = constants[CONSTANT_SWIPE_FLING_VELOCITY]
    val SWIPE_DURATION_MS: Long get() = constants[CONSTANT_SWIPE_SETTLE_DURATION_MS].toLong()

    /*
     * The core's drag constants: the held row's scale and the durations of the drag.
     */
    val LIFT_SCALE: Float get() = constants[CONSTANT_DRAG_LIFT_SCALE].toFloat()
    val LIFT_DURATION_MS: Long get() = constants[CONSTANT_DRAG_LIFT_DURATION_MS].toLong()
    val SHIFT_DURATION_MS: Long get() = constants[CONSTANT_DRAG_SHIFT_DURATION_MS].toLong()
    val DROP_DURATION_MS: Long get() = constants[CONSTANT_DRAG_DROP_DURATION_MS].toLong()

    /*
     * The core's section index sizes in dp.
     */
    val SECTION_INDEX_TITLE_HEIGHT_DP: Double get() = constants[CONSTANT_SECTION_INDEX_TITLE_HEIGHT]
    val SECTION_INDEX_WIDTH_DP: Double get() = constants[CONSTANT_SECTION_INDEX_WIDTH]

    /*
     * The core's default change animation duration.
     */
    val CHANGE_DURATION_MS: Long get() = constants[CONSTANT_CHANGE_DURATION_MS].toLong()

    /*
     * The core's smallest offset change that counts as a real move, in pixels.
     */
    val OFFSET_MOVED_THRESHOLD: Double get() = constants[CONSTANT_OFFSET_MOVED_THRESHOLD]

    /*
     * Slots of a step from runChange, then the kinds in the order of the core's ChangeStepKind.
     */
    const val CHANGE_STEP_KIND = 0
    const val CHANGE_STEP_FROM_X = 1
    const val CHANGE_STEP_FROM_Y = 2
    const val CHANGE_STEP_SLOTS = 3
    const val CHANGE_MOVE = 0
    const val CHANGE_INSERT = 1
    const val CHANGE_CARRY = 2

    @JvmStatic private external fun nativeDestroy(handle: Long)
    @JvmStatic private external fun nativeSetSettings(
      handle: Long, estimatedRowSize: Double, overscan: Double, startReachedThreshold: Double,
      endReachedThreshold: Double, numberOfColumns: Int, inverted: Boolean, followAppends: Boolean, horizontal: Boolean,
      snapToItem: Boolean, snapAlignment: Int)
    @JvmStatic private external fun nativeSetStickyIndices(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeReplaceKeys(handle: Long, start: Int, count: Int, packed: CharArray, ends: IntArray)
    @JvmStatic private external fun nativeInsertKeys(handle: Long, indices: IntArray, packed: CharArray, ends: IntArray)
    @JvmStatic private external fun nativeDeleteKeys(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeMarkRemeasure(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeResetKeepingPosition(handle: Long)
    @JvmStatic private external fun nativeRowCount(handle: Long): Int
    @JvmStatic private external fun nativeCopyRowRects(handle: Long, low: Int, high: Int, out: DoubleArray)
    @JvmStatic private external fun nativeRowRect(handle: Long, index: Int, out: DoubleArray): Boolean
    @JvmStatic private external fun nativeVisibleRange(handle: Long): Long
    @JvmStatic private external fun nativeIndexOfKey(handle: Long, key: String): Int
    @JvmStatic private external fun nativeCopyStickyFrames(handle: Long, out: DoubleArray): Boolean
    @JvmStatic private external fun nativeScrollToRow(handle: Long, row: Int, viewPosition: Double)
    @JvmStatic private external fun nativeScrollToStart(handle: Long)
    @JvmStatic private external fun nativeScrollToEnd(handle: Long)
    @JvmStatic private external fun nativeNearestSnapOffset(handle: Long, target: Double): Double
    @JvmStatic private external fun nativeAnimatedTargetOffset(
      handle: Long, index: Int, viewPosition: Double, windowAlong: Double, maxOffset: Double): Double
    @JvmStatic private external fun nativeSetLanding(handle: Long, target: Int, index: Int, viewPosition: Double)
    @JvmStatic private external fun nativeCancelLanding(handle: Long)
    @JvmStatic private external fun nativeLand(handle: Long): Boolean
    @JvmStatic private external fun nativeDragBegin(handle: Long, index: Int, touchAlong: Double, touchCross: Double)
    @JvmStatic private external fun nativeDragEnd(handle: Long)
    @JvmStatic private external fun nativeHeldIndex(handle: Long): Int
    @JvmStatic private external fun nativeDragFrame(
      handle: Long, touchAlong: Double, touchCross: Double, rows: IntArray, count: Int, out: DoubleArray): Int
    @JvmStatic private external fun nativeDragOriginIndex(handle: Long): Int
    @JvmStatic private external fun nativeDragInsertionIndex(handle: Long): Int
    @JvmStatic private external fun nativeDragAutoScrollOffset(
      touch: Double, windowSize: Double, offset: Double, maxOffset: Double, density: Double, dtSeconds: Double): Double
    @JvmStatic private external fun nativeSetSections(handle: Long, count: Int, counts: IntArray?, flags: IntArray?)
    @JvmStatic private external fun nativeCopyRows(handle: Long, items: IntArray, separators: BooleanArray, places: IntArray)
    @JvmStatic private external fun nativeItemForDrop(handle: Long, fromRow: Int, toRow: Int): Int
    @JvmStatic private external fun nativeRowForItem(handle: Long, item: Int): Int
    @JvmStatic private external fun nativeSectionForItem(handle: Long, item: Int): Int
    @JvmStatic private external fun nativeFirstItemInSection(handle: Long, section: Int): Int
    @JvmStatic private external fun nativeHeaderRow(handle: Long, section: Int): Int
    @JvmStatic private external fun nativeFooterRow(handle: Long, section: Int): Int
    @JvmStatic private external fun nativeFirstRowInSection(handle: Long, section: Int): Int
    @JvmStatic private external fun nativePlaceOfRow(handle: Long, row: Int): Int
    @JvmStatic private external fun nativeStickyRows(handle: Long, items: IntArray, sectionHeaders: Boolean): IntArray
    @JvmStatic private external fun nativeEdgeRowKeys(
      handle: Long, sectionKeys: Array<String?>, firstItemKeys: Array<String>): Array<String>
    @JvmStatic private external fun nativeUpdatePrefetch(handle: Long, low: Int, high: Int, out: IntArray): Int
    @JvmStatic private external fun nativeCopyPrefetch(handle: Long, out: IntArray)
    @JvmStatic private external fun nativeAnchorState(handle: Long, offset: Double, out: DoubleArray): String?
    @JvmStatic private external fun nativeRestoreAnchorState(handle: Long, key: String, offset: Double): Boolean
    @JvmStatic private external fun nativeDiffKeys(
      previousPacked: CharArray, previousEnds: IntArray, nextPacked: CharArray, nextEnds: IntArray, start: Int): IntArray
    @JvmStatic private external fun nativeCaptureChange(
      handle: Long, deletedPacked: CharArray, deletedEnds: IntArray, insertedPacked: CharArray, insertedEnds: IntArray,
      mountedPacked: CharArray, mountedEnds: IntArray, mountedPositions: DoubleArray)
    @JvmStatic private external fun nativeDeletedPosition(handle: Long, key: String, out: DoubleArray): Boolean
    @JvmStatic private external fun nativeRunChange(handle: Long, rows: IntArray, count: Int, positions: DoubleArray): DoubleArray?
    @JvmStatic private external fun nativeInsertionIndices(indices: IntArray, previousCount: Int): IntArray
    @JvmStatic private external fun nativeDeletionIndices(indices: IntArray, previousCount: Int): IntArray
    @JvmStatic private external fun nativeConstants(): DoubleArray
    @JvmStatic private external fun nativePageScrollTarget(
      offset: Double, windowAlong: Double, maxOffset: Double, direction: Int): Double
    @JvmStatic private external fun nativeSectionIndexTitlesTop(areaHeight: Double, count: Int, scale: Double): Double
    @JvmStatic private external fun nativeSectionIndexTitleAt(y: Double, areaHeight: Double, count: Int, scale: Double): Int
    @JvmStatic private external fun nativeSwipeButtonSize(fitted: Double, scale: Double): Double

    /*
     * The offset a held row near an edge scrolls the list to in a frame of dtSeconds.
     */
    fun dragAutoScrollOffset(
      touch: Double, windowSize: Double, offset: Double, maxOffset: Double, density: Double, dtSeconds: Double,
    ): Double = nativeDragAutoScrollOffset(touch, windowSize, offset, maxOffset, density, dtSeconds)

    /*
     * The diff of two key lists through the core's diffKeys, packed: the delete count and
     * previous indices, the insert count and next indices, the move count and from, to pairs.
     * Only the keys between the start unchanged keys and the end unchanged keys of both lists
     * cross. They change nothing in the diff.
     */
    fun diffKeys(
      previous: List<String>, next: List<String>, start: Int, end: Int,
      previousPacker: ShadowListKitKeyPacker, nextPacker: ShadowListKitKeyPacker,
    ): IntArray {
      val previousEnds = previousPacker.pack(previous, start, previous.size - end)
      val nextEnds = nextPacker.pack(next, start, next.size - end)
      return nativeDiffKeys(previousPacker.chars, previousEnds, nextPacker.chars, nextEnds, start)
    }

    /*
     * The core's insertionIndices and deletionIndices: sorted, each once, an insert past the
     * end at the end, a delete past the end dropped. Negative indices name no row.
     */
    fun insertionIndices(indices: IntArray, previousCount: Int): IntArray = nativeInsertionIndices(indices, previousCount)
    fun deletionIndices(indices: IntArray, previousCount: Int): IntArray = nativeDeletionIndices(indices, previousCount)

    /*
     * The core's swipeButtonSize: the title's fitted size with room around it, at least the
     * narrowest button, scaled to pixels.
     */
    fun swipeButtonSize(fitted: Double, scale: Double): Double = nativeSwipeButtonSize(fitted, scale)

    /*
     * The core's pageScrollTarget: one window toward the end or the start, inside the content.
     */
    fun pageScrollTarget(offset: Double, windowAlong: Double, maxOffset: Double, direction: Int): Double =
      nativePageScrollTarget(offset, windowAlong, maxOffset, direction)

    /*
     * The core's section index: where the titles start in the area, and the title under y
     * measured from the area's top.
     */
    fun sectionIndexTitlesTop(areaHeight: Double, count: Int, scale: Double): Double =
      nativeSectionIndexTitlesTop(areaHeight, count, scale)
    fun sectionIndexTitleAt(y: Double, areaHeight: Double, count: Int, scale: Double): Int =
      nativeSectionIndexTitleAt(y, areaHeight, count, scale)
  }

  /*
   * The native Peer, 0 once destroyed.
   */
  private var handle: Long = nativeCreate().also { reclaimer.register(this, it) }

  /*
   * The pass slots, see PASS_*. The view fills the inputs and reads the outputs.
   */
  val pass = DoubleArray(PASS_SLOTS)

  /*
   * Keys joined for the next key call, see replaceKeys.
   */
  private val packer = ShadowListKitKeyPacker()
  private val insertedPacker = ShadowListKitKeyPacker()
  private val mountedPacker = ShadowListKitKeyPacker()

  private external fun nativeCreate(): Long
  private external fun nativeRunPasses(handle: Long, io: DoubleArray)

  /*
   * Called by the driver during runPasses for every row in the measured range that has no size yet.
   * Only JNI calls it. Keep stops R8 in a minified app from removing or renaming it.
   */
  @Keep
  @Suppress("unused")
  private fun measureRow(row: Int, crossSize: Double): Double = measure(row, crossSize)

  /*
   * Free the native peer. The core is unusable after this.
   */
  fun destroy() {
    if (handle == 0L) return
    nativeDestroy(handle)
    reclaimer.release(handle)
    handle = 0L
  }

  fun setSettings(
    estimatedRowSize: Double, overscan: Double, startReachedThreshold: Double, endReachedThreshold: Double,
    numberOfColumns: Int, inverted: Boolean, followAppends: Boolean, horizontal: Boolean, snapToItem: Boolean,
    snapAlignment: Int,
  ) = nativeSetSettings(handle, estimatedRowSize, overscan, startReachedThreshold, endReachedThreshold, numberOfColumns,
    inverted, followAppends, horizontal, snapToItem, snapAlignment)

  fun setStickyIndices(indices: IntArray) = nativeSetStickyIndices(handle, indices)

  /*
   * Replace count keys from start with keys from until to. Keys cross as one char array and
   * one array of where each key ends, two JNI objects instead of one per key.
   */
  fun replaceKeys(start: Int, count: Int, keys: List<String>, from: Int, to: Int) {
    val ends = packer.pack(keys, from, to)
    nativeReplaceKeys(handle, start, count, packer.chars, ends)
  }

  fun insertKeys(indices: IntArray, keys: Array<String>) {
    val ends = packer.pack(keys.asList())
    nativeInsertKeys(handle, indices, packer.chars, ends)
  }

  fun deleteKeys(indices: IntArray) = nativeDeleteKeys(handle, indices)
  fun markRemeasure(indices: IntArray) = nativeMarkRemeasure(handle, indices)

  /*
   * Run the core with the inputs in pass until every row of the measured range has a size and
   * corrections landed.
   */
  fun runPasses() = nativeRunPasses(handle, pass)

  fun resetKeepingPosition() = nativeResetKeepingPosition(handle)
  val rowCount: Int get() = nativeRowCount(handle)
  fun copyRowRects(low: Int, high: Int, out: DoubleArray) = nativeCopyRowRects(handle, low, high, out)
  fun rowRect(index: Int, out: DoubleArray): Boolean = nativeRowRect(handle, index, out)

  /*
   * The rows overlapping the viewport as low shl 32 or high, or -1 before the first layout.
   */
  fun visibleRange(): Long = nativeVisibleRange(handle)

  fun indexOfKey(key: String): Int = nativeIndexOfKey(handle, key)
  fun copyStickyFrames(out: DoubleArray): Boolean = nativeCopyStickyFrames(handle, out)
  fun scrollToRow(row: Int, viewPosition: Double) = nativeScrollToRow(handle, row, viewPosition)
  fun scrollToStart() = nativeScrollToStart(handle)
  fun scrollToEnd() = nativeScrollToEnd(handle)
  fun nearestSnapOffset(target: Double): Double = nativeNearestSnapOffset(handle, target)

  /*
   * Where an animated scroll to a row aims, or NaN for a row not placed yet.
   */
  fun animatedTargetOffset(index: Int, viewPosition: Double, windowAlong: Double, maxOffset: Double): Double =
    nativeAnimatedTargetOffset(handle, index, viewPosition, windowAlong, maxOffset)

  fun setLanding(target: Int, index: Int = 0, viewPosition: Double = 0.0) =
    nativeSetLanding(handle, target, index, viewPosition)
  fun cancelLanding() = nativeCancelLanding(handle)

  /*
   * Send the landing's command after the animation ended. Returns whether there was one.
   */
  fun land(): Boolean = nativeLand(handle)

  fun dragBegin(index: Int, touchAlong: Double, touchCross: Double) =
    nativeDragBegin(handle, index, touchAlong, touchCross)
  fun dragEnd() = nativeDragEnd(handle)
  val heldIndex: Int get() = nativeHeldIndex(handle)

  /*
   * One drag frame: place the held row under the touch, find the drop slot among the first
   * count rows and write the DRAG_FRAME_* slots and each row's shift into out, which has room
   * for them. Returns the held row, or -1 when its key is gone.
   */
  fun dragFrame(touchAlong: Double, touchCross: Double, rows: IntArray, count: Int, out: DoubleArray): Int =
    nativeDragFrame(handle, touchAlong, touchCross, rows, count, out)
  val dragOriginIndex: Int get() = nativeDragOriginIndex(handle)
  val dragInsertionIndex: Int get() = nativeDragInsertionIndex(handle)

  /*
   * Sections over the rows, or none for null counts. flags are SECTION_* bits per section.
   */
  fun setSections(count: Int, counts: IntArray?, flags: IntArray?) = nativeSetSections(handle, count, counts, flags)

  /*
   * The item of every row, -1 for headers and footers, whether a separator follows the row and
   * its place as placeOfRow gives it.
   */
  fun copyRows(items: IntArray, separators: BooleanArray, places: IntArray) =
    nativeCopyRows(handle, items, separators, places)

  /*
   * The item a row dragged from fromRow and dropped at toRow becomes, or -1.
   */
  fun itemForDrop(fromRow: Int, toRow: Int): Int = nativeItemForDrop(handle, fromRow, toRow)
  fun rowForItem(item: Int): Int = nativeRowForItem(handle, item)
  fun sectionForItem(item: Int): Int = nativeSectionForItem(handle, item)
  fun firstItemInSection(section: Int): Int = nativeFirstItemInSection(handle, section)
  fun headerRow(section: Int): Int = nativeHeaderRow(handle, section)
  fun footerRow(section: Int): Int = nativeFooterRow(handle, section)
  fun firstRowInSection(section: Int): Int = nativeFirstRowInSection(handle, section)

  /*
   * Section shl ROW_KIND_BITS or a ROW_* kind, or -1 past the end.
   */
  fun placeOfRow(row: Int): Int = nativePlaceOfRow(handle, row)

  /*
   * The rows that stick: the sticky items' rows and with sectionHeaders every header row,
   * sorted, each once.
   */
  fun stickyRows(items: IntArray, sectionHeaders: Boolean): IntArray = nativeStickyRows(handle, items, sectionHeaders)

  /*
   * The keys of every header and footer row in row order. sectionKeys has the data source's key
   * of each section or null, firstItemKeys each section's first item key.
   */
  fun edgeRowKeys(sectionKeys: Array<String?>, firstItemKeys: Array<String>): Array<String> =
    nativeEdgeRowKeys(handle, sectionKeys, firstItemKeys)

  /*
   * Prefetch changes packed into prefetchOut: the prefetch count and items, then the cancel
   * count and items. The array grows when they need more room.
   */
  var prefetchOut = IntArray(64)
    private set

  fun updatePrefetch(low: Int, high: Int) {
    val length = nativeUpdatePrefetch(handle, low, high, prefetchOut)
    if (length <= prefetchOut.size) return
    prefetchOut = IntArray(max(length, prefetchOut.size * 2))
    nativeCopyPrefetch(handle, prefetchOut)
  }

  /*
   * The key of the row at the viewport start, with the distance into it in out[0], or null.
   */
  fun anchorState(offset: Double, out: DoubleArray): String? = nativeAnchorState(handle, offset, out)
  fun restoreAnchorState(key: String, offset: Double): Boolean = nativeRestoreAnchorState(handle, key, offset)

  /*
   * The core's ChangeAnimation: a change about to reach the core, with the mounted rows and
   * where they show as x, y pairs. They are recorded when the change starts an animation.
   */
  fun captureChange(deleted: List<String>, inserted: List<String>, mounted: List<String>, positions: DoubleArray) {
    val deletedEnds = packer.pack(deleted)
    val insertedEnds = insertedPacker.pack(inserted)
    val mountedEnds = mountedPacker.pack(mounted)
    nativeCaptureChange(handle, packer.chars, deletedEnds, insertedPacker.chars, insertedEnds, mountedPacker.chars,
      mountedEnds, positions)
  }

  /*
   * Where a deleted row showed before the change into out, or false when it does not fade out.
   */
  fun deletedPosition(key: String, out: DoubleArray): Boolean = nativeDeletedPosition(handle, key, out)

  /*
   * The animation of the first count rows, mounted and low to high, CHANGE_STEP_SLOTS values
   * each, or null without a pending change. The core reads their keys itself.
   */
  fun runChange(rows: IntArray, count: Int, positions: DoubleArray): DoubleArray? =
    nativeRunChange(handle, rows, count, positions)
}

/*
 * Keys joined into one char array with where each one ends, the way the JNI reads them. The
 * array is reused and grows as needed. Every call runs on the UI thread.
 */
internal class ShadowListKitKeyPacker {
  var chars = CharArray(1024)
    private set

  /*
   * Join keys from until to into chars. Returns where each key ends in it.
   */
  fun pack(keys: List<String>, from: Int = 0, to: Int = keys.size): IntArray {
    val ends = IntArray(max(0, to - from))
    var length = 0
    for (position in from until to) length += keys[position].length
    if (chars.size < length) chars = CharArray(max(length, chars.size * 2))
    var at = 0
    for (position in from until to) {
      val key = keys[position]
      key.toCharArray(chars, at, 0, key.length)
      at += key.length
      ends[position - from] = at
    }
    return ends
  }
}
