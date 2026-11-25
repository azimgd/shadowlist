package com.shadowlist.kit

import android.view.View
import android.view.ViewGroup
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

  var overlayView: View? = null
    private set
  private var overlayCell: ShadowListKitListCell? = null
  private var overlayCellVisibility = View.VISIBLE
  private var overlayCellAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
  private var overlayKey: String? = null
  private var overlayRevision = 0L
  private var renderedOverlayRevision = -1L

  fun invalidateOverlay() {
    ++overlayRevision
  }

  fun clearOverlay() {
    ++overlayRevision
    if (overlayView == null && overlayCell == null && overlayKey == null) return
    overlayView?.let { (it.parent as? ViewGroup)?.removeView(it) }
    overlayView = null
    overlayCell?.let {
      it.visibility = overlayCellVisibility
      it.importantForAccessibility = overlayCellAccessibility
    }
    overlayCell = null
    overlayKey = null
    list.contentStale = true
  }

  fun recyclingCell(cell: ShadowListKitListCell) {
    if (cell === overlayCell) clearOverlay()
  }

  private fun layoutOverlay(active: Int, cell: ShadowListKitListCell?): Boolean {
    val factory = list.renderStickyHeaderOverlay
    if (factory == null || cell == null || active !in list.keys.indices) {
      clearOverlay()
      return false
    }
    val key = list.keys[active]
    if (overlayKey != key || renderedOverlayRevision != overlayRevision) {
      clearOverlay()
      val revision = overlayRevision
      val place = list.placeOfRow(active)
      val index = list.itemForRow(active)
      val section = if (list.isSectioned && place >= 0) place shr ShadowListKitCore.ROW_KIND_BITS else -1
      val source = list.dataSource
      val publicKey = if (index >= 0) {
        source?.keyForItem(list, index)
      } else {
        val sections = source as? ShadowListKitListView.Sections
        sections?.keyForSection(list, section)
          ?: if (sections != null && sections.numberOfItemsInSection(list, section) > 0) {
            source?.keyForItem(list, list.firstItemIndexInSection(section))
          } else {
            null
          }
      }
      val view = factory(publicKey, index, section)
      if (revision != overlayRevision || factory !== list.renderStickyHeaderOverlay) return false
      overlayKey = key
      renderedOverlayRevision = revision
      overlayView = view
      if (view != null) {
        overlayCell = cell
        overlayCellVisibility = cell.visibility
        overlayCellAccessibility = cell.importantForAccessibility
        cell.visibility = View.INVISIBLE
        cell.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
        (view.parent as? ViewGroup)?.removeView(view)
        list.addView(view)
        list.contentStale = true
      }
    }
    val view = overlayView ?: return false
    val core = list.liveCore ?: return false
    val extent = list.templateSize(view, list.windowCross)
    val leading = core.stickyLeading(active, list.exactOffset, extent.toDouble())
    val cross = list.windowCross.toDouble()
    if (list.horizontal) {
      list.placeView(view, leading, 0.0, extent.toDouble(), cross)
    } else {
      list.placeView(view, 0.0, leading, cross, extent.toDouble())
    }
    return true
  }

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
   * The core's pinnedSectionIndex over the copied frames: the position in stickyRows of the last
   * sticky row starting at or above the offset, or -1. Rows the core has not placed yet start at
   * infinity and sort last. A scroll frame makes no JNI call for it. The core test
   * list_driver_sticky_frames_pin_the_same_header_as_the_driver holds the rule this and
   * pinnedSectionLeading match.
   */
  private fun pinnedSectionIndex(offset: Double): Int {
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

  fun activeStickyIndex(offset: Double): Int = pinnedSectionIndex(offset).let { if (it >= 0) stickyRows[it] else -1 }

  /*
   * The core's pinnedSectionLeading: where the pinned header's leading edge goes, at the offset,
   * never above its own place, and pushed up by the next header when the core placed it.
   */
  private fun pinnedSectionLeading(position: Int, offset: Double): Double {
    var pinned = max(stickyFrames[position * 2], offset)
    val next = position + 1
    if (next < stickyRows.size && stickyFrames[next * 2].isFinite()) {
      pinned = min(pinned, stickyFrames[next * 2] - stickyFrames[position * 2 + 1])
    }
    return pinned
  }

  /*
   * Pin the active section header and put the one it replaced back in its row. With no sticky
   * rows left, the row pinned last goes back to its own place.
   */
  fun layoutSticky() {
    if (stickyRows.isEmpty()) {
      clearOverlay()
      stickyCell?.let { unpinCell(it) }
      stickyCell = null
      return
    }
    val position = pinnedSectionIndex(list.exactOffset)
    val active = if (position >= 0) stickyRows[position] else -1
    // A held sticky item follows the finger from its own row.
    val cell = list.mountedCell(active)?.takeIf { it !== list.heldCell }
    val previous = stickyCell
    if (previous != null && previous !== cell) unpinCell(previous)
    stickyCell = cell
    if (layoutOverlay(active, cell)) return
    if (cell == null || cell.row != active) return
    val pinned = pinnedSectionLeading(position, list.exactOffset)
    val extent = stickyFrames[position * 2 + 1]
    val cross = list.windowCross.toDouble()
    if (list.horizontal) {
      list.placeView(
        cell,
        pinned,
        0.0,
        extent,
        cross
      )
    } else {
      list.placeView(cell, 0.0, pinned, cross, extent)
    }
  }

  private fun unpinCell(cell: ShadowListKitListCell) {
    if (cell.row in list.keys.indices) list.placeCell(cell, cell.row)
  }
}
