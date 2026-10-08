package com.shadowlist.kit

import kotlin.math.max

/*
 * The shadowlist core behind one list, through the core's ListDriver in C++. Every call runs
 * on the UI thread. Pass values go in and out through arrays this object reuses.
 */
internal class SLKCore(private val measure: (index: Int, crossSize: Double) -> Double) {
  companion object {
    init {
      System.loadLibrary("shadowlistkit")
    }

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
    const val PASS_OUT_WINDOW_LOW = 16
    const val PASS_OUT_WINDOW_HIGH = 17
    const val PASS_SLOTS = 18

    // Values of PASS_PHASE, the same as the core's ScrollPhase.
    const val PHASE_IDLE = 0
    const val PHASE_DRAGGING = 1
    const val PHASE_SETTLING = 2

    // Where an animated scroll command lands, the same as the core's ScrollLanding::Target.
    const val LANDING_INDEX = 1
    const val LANDING_START = 2
    const val LANDING_END = 3

    // Bits of a section's flags in setSections.
    const val SECTION_HEADER = 1
    const val SECTION_FOOTER = 2

    // Kinds of a row in placeOfRow, the same order as the core's RowKind.
    const val ROW_ITEM = 0
    const val ROW_HEADER = 1
    const val ROW_FOOTER = 2

    // Slots of a swipe array, the spec then where a released swipe rests.
    const val SWIPE_LEADING_WIDTH = 0
    const val SWIPE_TRAILING_WIDTH = 1
    const val SWIPE_LEADING_FULL = 2
    const val SWIPE_TRAILING_FULL = 3
    const val SWIPE_ROW_SIZE = 4
    const val SWIPE_OUT_SIDE = 5
    const val SWIPE_OUT_FULL = 6
    const val SWIPE_OUT_OFFSET = 7
    const val SWIPE_SLOTS = 8

    // Values of SWIPE_OUT_SIDE, the same order as the core's SwipeSide.
    const val SWIPE_SIDE_NONE = 0
    const val SWIPE_SIDE_LEADING = 1
    const val SWIPE_SIDE_TRAILING = 2

    // Prefixes of section header and footer row keys, the same as the core's ListSections.
    const val HEADER_KEY_PREFIX = "\u001fh:"
    const val FOOTER_KEY_PREFIX = "\u001ff:"

    @JvmStatic private external fun nativeDestroy(handle: Long)
    @JvmStatic private external fun nativeSetSettings(
      handle: Long, estimatedItemSize: Double, overscan: Double, startReachedThreshold: Double,
      endReachedThreshold: Double, columns: Int, inverted: Boolean, followAppends: Boolean, horizontal: Boolean,
      snapToItem: Boolean, snapAlignment: Int)
    @JvmStatic private external fun nativeSetStickyIndices(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeReplaceKeys(handle: Long, start: Int, count: Int, packed: CharArray, ends: IntArray)
    @JvmStatic private external fun nativeInsertKeys(handle: Long, indices: IntArray, packed: CharArray, ends: IntArray)
    @JvmStatic private external fun nativeDeleteKeys(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeMarkRemeasure(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeResetKeepingPosition(handle: Long)
    @JvmStatic private external fun nativeCount(handle: Long): Int
    @JvmStatic private external fun nativeCopyRowRects(handle: Long, low: Int, high: Int, out: DoubleArray)
    @JvmStatic private external fun nativeRowRect(handle: Long, index: Int, out: DoubleArray): Boolean
    @JvmStatic private external fun nativeFooterStart(handle: Long, footerSize: Double): Double
    @JvmStatic private external fun nativeVisibleRange(handle: Long): Long
    @JvmStatic private external fun nativeIndexOfKey(handle: Long, key: String): Int
    @JvmStatic private external fun nativeCopyStickyFrames(handle: Long, out: DoubleArray): Boolean
    @JvmStatic private external fun nativeScrollToIndex(handle: Long, index: Int, viewPosition: Double)
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
    @JvmStatic private external fun nativePlaceHeld(
      handle: Long, held: Int, touchAlong: Double, touchCross: Double, out: DoubleArray)
    @JvmStatic private external fun nativeDragUpdateInsertion(handle: Long, mounted: IntArray)
    @JvmStatic private external fun nativeDragShiftFor(handle: Long, index: Int, out: DoubleArray)
    @JvmStatic private external fun nativeDragOriginIndex(handle: Long): Int
    @JvmStatic private external fun nativeDragInsertionIndex(handle: Long): Int
    @JvmStatic private external fun nativeDragAutoScrollOffset(
      touch: Double, windowSize: Double, offset: Double, maxOffset: Double, density: Double): Double
    @JvmStatic private external fun nativeSetSections(handle: Long, count: Int, counts: IntArray?, flags: IntArray?)
    @JvmStatic private external fun nativeCopyRows(handle: Long, items: IntArray, separators: BooleanArray)
    @JvmStatic private external fun nativeItemForDrop(handle: Long, fromRow: Int, toRow: Int): Int
    @JvmStatic private external fun nativeRowForItem(handle: Long, item: Int): Int
    @JvmStatic private external fun nativeSectionForItem(handle: Long, item: Int): Int
    @JvmStatic private external fun nativeFirstItemInSection(handle: Long, section: Int): Int
    @JvmStatic private external fun nativeHeaderRow(handle: Long, section: Int): Int
    @JvmStatic private external fun nativeFirstRowInSection(handle: Long, section: Int): Int
    @JvmStatic private external fun nativePlaceOfRow(handle: Long, row: Int): Int
    @JvmStatic private external fun nativeHeaderRows(handle: Long): IntArray
    @JvmStatic private external fun nativeUpdatePrefetch(handle: Long, low: Int, high: Int): IntArray
    @JvmStatic private external fun nativeAnchor(handle: Long, offset: Double, out: DoubleArray): String?
    @JvmStatic private external fun nativeRestoreAnchor(handle: Long, key: String, offset: Double): Boolean
    @JvmStatic private external fun nativeDiffKeys(
      previousPacked: CharArray, previousEnds: IntArray, nextPacked: CharArray, nextEnds: IntArray): IntArray
    @JvmStatic private external fun nativePlanBatch(
      previousCount: Int, nextCount: Int, deleted: IntArray, inserted: IntArray, movedFrom: IntArray, movedTo: IntArray,
    ): IntArray?
    @JvmStatic private external fun nativeSwipeDrag(io: DoubleArray, start: Double, translation: Double): Double
    @JvmStatic private external fun nativeSwipePastFull(io: DoubleArray, offset: Double): Boolean
    @JvmStatic private external fun nativeSwipeSettle(io: DoubleArray, offset: Double, velocity: Double, flingVelocity: Double)

    /*
     * The offset a held row near an edge scrolls the list to this frame.
     */
    fun dragAutoScrollOffset(touch: Double, windowSize: Double, offset: Double, maxOffset: Double, density: Double): Double =
      nativeDragAutoScrollOffset(touch, windowSize, offset, maxOffset, density)

    /*
     * The diff of two key lists through the core's diffKeys, packed: the delete count and
     * previous indices, the insert count and next indices, the move count and from, to pairs.
     */
    fun diffKeys(previous: List<String>, next: List<String>): IntArray {
      val (previousPacked, previousEnds) = packed(previous)
      val (nextPacked, nextEnds) = packed(next)
      return nativeDiffKeys(previousPacked, previousEnds, nextPacked, nextEnds)
    }

    private fun packed(keys: List<String>): Pair<CharArray, IntArray> {
      var length = 0
      for (key in keys) length += key.length
      val chars = CharArray(length)
      val ends = IntArray(keys.size)
      var at = 0
      for ((position, key) in keys.withIndex()) {
        key.toCharArray(chars, at, 0, key.length)
        at += key.length
        ends[position] = at
      }
      return chars to ends
    }

    /*
     * The source of every row after a batch, a previous index or -1 for an insert, or null when
     * the batch does not add up. See the core's planBatch.
     */
    fun planBatch(
      previousCount: Int, nextCount: Int, deleted: IntArray, inserted: IntArray, movedFrom: IntArray, movedTo: IntArray,
    ): IntArray? = nativePlanBatch(previousCount, nextCount, deleted, inserted, movedFrom, movedTo)

    // The core's SwipeReveal over a SWIPE_* array.
    fun swipeDrag(io: DoubleArray, start: Double, translation: Double): Double = nativeSwipeDrag(io, start, translation)
    fun swipePastFull(io: DoubleArray, offset: Double): Boolean = nativeSwipePastFull(io, offset)
    fun swipeSettle(io: DoubleArray, offset: Double, velocity: Double, flingVelocity: Double) =
      nativeSwipeSettle(io, offset, velocity, flingVelocity)
  }

  // The native Peer, 0 once destroyed.
  private var handle: Long = nativeCreate()

  // The pass slots, see PASS_*. The view fills the inputs and reads the outputs.
  val pass = DoubleArray(PASS_SLOTS)

  // Keys joined for the next key call, see replaceKeys. Reused and grown as needed.
  private var packed = CharArray(1024)

  private external fun nativeCreate(): Long
  private external fun nativeRunPasses(handle: Long, io: DoubleArray)

  /*
   * Called by the driver during runPasses for every row in the window that has no size yet.
   */
  @Suppress("unused")
  private fun measureItem(index: Int, crossSize: Double): Double = measure(index, crossSize)

  /*
   * Free the native peer. The core is unusable after this.
   */
  fun destroy() {
    if (handle == 0L) return
    nativeDestroy(handle)
    handle = 0L
  }

  fun setSettings(
    estimatedItemSize: Double, overscan: Double, startReachedThreshold: Double, endReachedThreshold: Double,
    columns: Int, inverted: Boolean, followAppends: Boolean, horizontal: Boolean, snapToItem: Boolean,
    snapAlignment: Int,
  ) = nativeSetSettings(handle, estimatedItemSize, overscan, startReachedThreshold, endReachedThreshold, columns,
    inverted, followAppends, horizontal, snapToItem, snapAlignment)

  fun setStickyIndices(indices: IntArray) = nativeSetStickyIndices(handle, indices)

  /*
   * Replace count keys from start with keys from until to. Keys cross as one char array and
   * one array of where each key ends, two JNI objects instead of one per key.
   */
  fun replaceKeys(start: Int, count: Int, keys: List<String>, from: Int, to: Int) {
    val ends = pack(keys, from, to)
    nativeReplaceKeys(handle, start, count, packed, ends)
  }

  fun insertKeys(indices: IntArray, keys: Array<String>) {
    val ends = pack(keys.asList(), 0, keys.size)
    nativeInsertKeys(handle, indices, packed, ends)
  }

  /*
   * Join keys from until to into packed. Returns where each key ends in it.
   */
  private fun pack(keys: List<String>, from: Int, to: Int): IntArray {
    val ends = IntArray(max(0, to - from))
    var length = 0
    for (position in from until to) length += keys[position].length
    if (packed.size < length) packed = CharArray(max(length, packed.size * 2))
    var at = 0
    for (position in from until to) {
      val key = keys[position]
      key.toCharArray(packed, at, 0, key.length)
      at += key.length
      ends[position - from] = at
    }
    return ends
  }

  fun deleteKeys(indices: IntArray) = nativeDeleteKeys(handle, indices)
  fun markRemeasure(indices: IntArray) = nativeMarkRemeasure(handle, indices)

  /*
   * Run the core with the inputs in pass until the window is measured and corrections landed.
   */
  fun runPasses() = nativeRunPasses(handle, pass)

  fun resetKeepingPosition() = nativeResetKeepingPosition(handle)
  val count: Int get() = nativeCount(handle)
  fun copyRowRects(low: Int, high: Int, out: DoubleArray) = nativeCopyRowRects(handle, low, high, out)
  fun rowRect(index: Int, out: DoubleArray): Boolean = nativeRowRect(handle, index, out)
  fun footerStart(footerSize: Double): Double = nativeFooterStart(handle, footerSize)

  /*
   * The visible rows as low shl 32 or high, or -1 before the first layout.
   */
  fun visibleRange(): Long = nativeVisibleRange(handle)

  fun indexOfKey(key: String): Int = nativeIndexOfKey(handle, key)
  fun copyStickyFrames(out: DoubleArray): Boolean = nativeCopyStickyFrames(handle, out)
  fun scrollToIndex(index: Int, viewPosition: Double) = nativeScrollToIndex(handle, index, viewPosition)
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
  fun placeHeld(held: Int, touchAlong: Double, touchCross: Double, out: DoubleArray) =
    nativePlaceHeld(handle, held, touchAlong, touchCross, out)
  fun dragUpdateInsertion(mounted: IntArray) = nativeDragUpdateInsertion(handle, mounted)
  fun dragShiftFor(index: Int, out: DoubleArray) = nativeDragShiftFor(handle, index, out)
  val dragOriginIndex: Int get() = nativeDragOriginIndex(handle)
  val dragInsertionIndex: Int get() = nativeDragInsertionIndex(handle)

  /*
   * Sections over the rows, or none for null counts. flags are SECTION_* bits per section.
   */
  fun setSections(count: Int, counts: IntArray?, flags: IntArray?) = nativeSetSections(handle, count, counts, flags)
  /*
   * The item of every row, -1 for headers and footers, and whether a separator follows the row.
   */
  fun copyRows(items: IntArray, separators: BooleanArray) = nativeCopyRows(handle, items, separators)

  /*
   * The item a row dragged from fromRow and dropped at toRow becomes, or -1.
   */
  fun itemForDrop(fromRow: Int, toRow: Int): Int = nativeItemForDrop(handle, fromRow, toRow)
  fun rowForItem(item: Int): Int = nativeRowForItem(handle, item)
  fun sectionForItem(item: Int): Int = nativeSectionForItem(handle, item)
  fun firstItemInSection(section: Int): Int = nativeFirstItemInSection(handle, section)
  fun headerRow(section: Int): Int = nativeHeaderRow(handle, section)
  fun firstRowInSection(section: Int): Int = nativeFirstRowInSection(handle, section)

  /*
   * Section shl 2 or a ROW_* kind, or -1 past the end.
   */
  fun placeOfRow(row: Int): Int = nativePlaceOfRow(handle, row)
  fun headerRows(): IntArray = nativeHeaderRows(handle)

  /*
   * Prefetch changes, packed: the prefetch count and rows, then the cancel count and rows.
   */
  fun updatePrefetch(low: Int, high: Int): IntArray = nativeUpdatePrefetch(handle, low, high)

  /*
   * The key of the row at the viewport start, with the distance into it in out[0], or null.
   */
  fun anchor(offset: Double, out: DoubleArray): String? = nativeAnchor(handle, offset, out)
  fun restoreAnchor(key: String, offset: Double): Boolean = nativeRestoreAnchor(handle, key, offset)
}
