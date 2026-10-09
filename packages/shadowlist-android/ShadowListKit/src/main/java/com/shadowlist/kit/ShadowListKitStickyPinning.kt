package com.shadowlist.kit

import kotlin.math.max
import kotlin.math.min

/*
 * Pins the active sticky row, a sticky item or a section header, at the viewport start and puts
 * the one it replaced back in its row. The sticky frames are copied from the core when the
 * geometry changes. A scroll frame reads only the copy and makes no JNI call.
 */
internal class ShadowListKitStickyPinning(private val list: ShadowListKitListView) {
  var stickyCell: ShadowListKitListCell? = null
    private set

  /*
   * The sticky rows: the sticky items' rows and the section headers, sorted.
   */
  var stickyRows = IntArray(0)
    private set

  /*
   * Leading edge and extent of every sticky row, copied from the core when the geometry
   * changes. Pinning reads only these on a scroll frame.
   */
  private var stickyFrames = DoubleArray(0)
  private var stickyGeometry = -1.0

  /*
   * Read the sticky rows again. Returns whether they changed.
   */
  fun updateStickyRows(stickyIndices: IntArray, stickySectionHeaders: Boolean): Boolean {
    val sorted = list.core.stickyRows(stickyIndices, stickySectionHeaders)
    if (sorted.contentEquals(stickyRows)) return false
    stickyRows = sorted
    list.liveCore?.setStickyIndices(sorted)
    stickyGeometry = -1.0
    return true
  }

  /*
   * Copy the frames again on the next pin, like after a new core.
   */
  fun invalidateFrames() {
    stickyGeometry = -1.0
  }

  private fun refreshStickyFrames() {
    if (stickyGeometry == list.frameGeometry) return
    if (stickyFrames.size < stickyRows.size * 2) stickyFrames = DoubleArray(stickyRows.size * 2)
    if (list.core.copyStickyFrames(stickyFrames)) stickyGeometry = list.frameGeometry
  }

  /*
   * Position in stickyRows of the last sticky row starting at or above the offset, or -1. Rows
   * the core has not placed yet start at infinity and sort last. The core test
   * list_driver_sticky_frames_pin_the_same_header_as_the_driver holds the rule this and
   * stickyLeading match.
   */
  private fun activeStickyPosition(offset: Double): Int {
    if (stickyRows.isEmpty()) return -1
    refreshStickyFrames()
    var found = -1
    var low = 0
    var high = stickyRows.size
    while (low < high) {
      val mid = (low + high) ushr 1
      if (stickyFrames[mid * 2] <= offset) {
        found = mid
        low = mid + 1
      } else {
        high = mid
      }
    }
    return found
  }

  fun activeStickyIndex(offset: Double): Int =
    activeStickyPosition(offset).let { if (it >= 0) stickyRows[it] else -1 }

  /*
   * Where the pinned header's leading edge goes, pushed up by the next header.
   */
  private fun stickyLeading(position: Int, offset: Double): Double {
    var pinned = max(stickyFrames[position * 2], offset)
    val next = position + 1
    if (next < stickyRows.size && stickyFrames[next * 2].isFinite()) {
      pinned = min(pinned, stickyFrames[next * 2] - stickyFrames[position * 2 + 1])
    }
    return pinned
  }

  /*
   * Pin the active section header and put the one it replaced back in its row.
   */
  fun layoutSticky() {
    if (stickyRows.isEmpty()) return
    val position = activeStickyPosition(list.offset.toDouble())
    val active = if (position >= 0) stickyRows[position] else -1
    val cell = list.mountedCell(active)
    val previous = stickyCell
    if (previous != null && previous !== cell) unpinCell(previous)
    stickyCell = cell
    if (cell == null) return
    val pinned = stickyLeading(position, list.offset.toDouble())
    val extent = stickyFrames[position * 2 + 1]
    val cross = list.windowCross.toDouble()
    if (list.horizontal) list.placeView(cell, pinned, 0.0, extent, cross) else list.placeView(cell, 0.0, pinned, cross, extent)
  }

  private fun unpinCell(cell: ShadowListKitListCell) {
    if (cell.row in list.keys.indices) list.placeCell(cell, cell.row)
  }
}
