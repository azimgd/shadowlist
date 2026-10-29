package com.shadowlist.kit

import kotlin.math.max

/*
 * The core's SwipeReveal of the row swiped open, held in a native peer with both sides' button
 * sizes. A touch move or an animation frame is one JNI call, which writes the row's offset,
 * whether it is past the full swipe point and where the buttons go into out. Every call runs on
 * the UI thread.
 */
internal class ShadowListKitSwipeReveal {
  companion object {
    init {
      System.loadLibrary("shadowlistkit")
    }

    /*
     * Frees the peer once its list is collected. A list has no call that ends it.
     */
    private val reclaimer = ShadowListKitPeerReclaimer(::nativeDestroy)

    /*
     * Slots of out, the same as SwipeOutSlot in the JNI. Each button's start and size follow from
     * OUT_SPANS, the first action at the outer edge.
     */
    const val OUT_OFFSET = 0
    const val OUT_PAST_FULL = 1
    const val OUT_REVEALED_START = 2
    const val OUT_REVEALED_SIZE = 3
    const val OUT_SPANS = 4

    /*
     * Values of restSide, the same order as the core's SwipeSide.
     */
    const val SIDE_NONE = 0
    const val SIDE_LEADING = 1
    const val SIDE_TRAILING = 2

    private const val REST_SIDE = 0
    private const val REST_FULL = 1
    private const val REST_OFFSET = 2

    @JvmStatic private external fun nativeCreate(): Long
    @JvmStatic private external fun nativeDestroy(handle: Long)
    @JvmStatic private external fun nativeBegin(
      handle: Long, leadingSizes: DoubleArray, trailingSizes: DoubleArray, leadingFull: Boolean, trailingFull: Boolean,
      rowSize: Double)
    @JvmStatic private external fun nativeMove(handle: Long, start: Double, translation: Double, crossSize: Double, out: DoubleArray)
    @JvmStatic private external fun nativeShow(handle: Long, offset: Double, crossSize: Double, out: DoubleArray)
    @JvmStatic private external fun nativeIsSwipedOut(handle: Long, offset: Double): Boolean
    @JvmStatic private external fun nativeSettle(handle: Long, offset: Double, velocity: Double, flingVelocity: Double, out: DoubleArray)
  }

  private val handle: Long = nativeCreate().also { reclaimer.register(this, it) }
  private val rest = DoubleArray(3)

  /*
   * What the last move or show wrote, see OUT_*.
   */
  var out = DoubleArray(OUT_SPANS)
    private set

  /*
   * A row with these buttons opens. rowSize is its cross size.
   */
  fun begin(leadingSizes: DoubleArray, trailingSizes: DoubleArray, leadingFull: Boolean, trailingFull: Boolean, rowSize: Double) {
    val needed = OUT_SPANS + 2 * max(leadingSizes.size, trailingSizes.size)
    if (out.size < needed) out = DoubleArray(needed)
    nativeBegin(handle, leadingSizes, trailingSizes, leadingFull, trailingFull, rowSize)
  }

  /*
   * A finger that moved translation since the swipe began at start, the buttons laid out across
   * crossSize.
   */
  fun move(start: Double, translation: Double, crossSize: Double) = nativeMove(handle, start, translation, crossSize, out)

  /*
   * The row at offset, like an animation frame.
   */
  fun show(offset: Double, crossSize: Double) = nativeShow(handle, offset, crossSize, out)

  fun isSwipedOut(offset: Double): Boolean = nativeIsSwipedOut(handle, offset)

  /*
   * Where a swipe let go at offset with velocity rests. Read it from restSide, restFull and
   * restOffset.
   */
  fun settle(offset: Double, velocity: Double, flingVelocity: Double) = nativeSettle(handle, offset, velocity, flingVelocity, rest)

  val restSide: Int get() = rest[REST_SIDE].toInt()
  val restFull: Boolean get() = rest[REST_FULL] != 0.0
  val restOffset: Double get() = rest[REST_OFFSET]
}
