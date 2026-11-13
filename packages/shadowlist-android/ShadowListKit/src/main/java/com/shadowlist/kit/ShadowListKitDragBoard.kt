package com.shadowlist.kit

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.os.SystemClock
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import java.lang.ref.WeakReference
import java.util.Collections
import java.util.WeakHashMap
import kotlin.math.abs
import kotlin.math.roundToInt

/*
 * The shared DragBoard owns the flight. Android keeps weak list membership and a picture in
 * the root overlay. Points handed to the core are screen coordinates on both sides.
 */
internal class ShadowListKitDragBoard private constructor() {
  private var handle = ShadowListKitCore.nativeDragBoardCreate()
  private val lists = Collections.newSetFromMap(WeakHashMap<ShadowListKitListView, Boolean>())
  private var home = WeakReference<ShadowListKitListView>(null)
  private var target = WeakReference<ShadowListKitListView>(null)
  private var overlay: ImageView? = null
  private var overlayParent: ViewGroup? = null
  private var landingCell = WeakReference<ShadowListKitListCell>(null)
  private var heldKey = ""
  private val frame = DoubleArray(ShadowListKitCore.BOARD_FRAME_SLOTS)
  private val release = DoubleArray(ShadowListKitCore.BOARD_RELEASE_SLOTS)
  private val handoff = DoubleArray(POINT_SLOTS)
  private val location = IntArray(2)
  private var fingerWindowX = 0f
  private var fingerWindowY = 0f
  private var sidewaysCarry = 0.0
  private var sidewaysScrollView = WeakReference<ShadowListKitListView>(null)

  init {
    reclaimer.register(this, handle)
  }

  fun leave(list: ShadowListKitListView) {
    if (handle == 0L) return
    if (home.get() === list || landingCell.get()?.parent === list) endFlight()
    else if (target.get() === list) {
      endForeign(list)
      target.clear()
    }
    ShadowListKitCore.nativeDragBoardLeave(handle, list.dragListId.orEmpty())
    lists.remove(list)
    list.dragBoard = null
    if (lists.isEmpty()) {
      ShadowListKitCore.nativeDragBoardDestroy(handle)
      reclaimer.release(handle)
      handle = 0
      boards.entries.removeAll { it.value.get() === this }
    }
  }

  fun begin(list: ShadowListKitListView, cell: ShadowListKitListCell, x: Float, y: Float) {
    if (list.isSectioned) return
    endFlight()
    home = WeakReference(list)
    heldKey = list.keys.getOrNull(cell.row) ?: return
    list.getLocationOnScreen(location)
    val fingerX = location[POINT_X] + x
    val fingerY = location[POINT_Y] + y
    fingerWindowX = fingerX
    fingerWindowY = fingerY
    sidewaysCarry = 0.0
    cell.getLocationOnScreen(location)
    ShadowListKitCore.nativeDragBoardBegin(handle, list.dragListId.orEmpty(), heldKey, fingerX.toDouble(), fingerY.toDouble(),
      location[POINT_X] + cell.width / 2.0, location[POINT_Y] + cell.height / 2.0)
  }

  fun update(list: ShadowListKitListView, cell: ShadowListKitListCell): Boolean {
    if (home.get() !== list) return false
    val fingerX = fingerWindowX
    val fingerY = fingerWindowY
    val hovered = hitList(list.rootView, fingerX.toInt(), fingerY.toInt())?.takeIf { it.reorderEnabled && !it.isSectioned }
    cell.getLocationOnScreen(location)
    ShadowListKitCore.nativeDragBoardFrame(handle, fingerX.toDouble(), fingerY.toDouble(),
      location[POINT_X] + cell.width / 2.0, location[POINT_Y] + cell.height / 2.0,
      hovered?.dragListId.orEmpty(), now(), frame)
    val floating = frame[ShadowListKitCore.BOARD_FRAME_FLOATING] != 0.0
    if (frame[ShadowListKitCore.BOARD_FRAME_STEP].toInt() != ShadowListKitCore.BOARD_STAY) {
      endForeign(target.get())
      target = WeakReference(if (floating) hovered else null)
      list.core.dragSetHeldAway(floating)
      target.get()?.let {
        it.core.dragBeginForeign((if (it.horizontal) cell.width else cell.height).toDouble(),
          (if (it.horizontal) cell.height else cell.width).toDouble())
      }
    }
    if (floating && overlay == null) {
      val parent = list.rootView as? ViewGroup ?: return false
      val bitmap = Bitmap.createBitmap(cell.width.coerceAtLeast(1), cell.height.coerceAtLeast(1), Bitmap.Config.ARGB_8888)
      cell.draw(Canvas(bitmap))
      overlay = ImageView(list.context).apply {
        setImageBitmap(bitmap)
        layout(0, 0, cell.width, cell.height)
      }
      overlayParent = parent
      parent.overlay.add(overlay!!)
    }
    cell.alpha = if (frame[ShadowListKitCore.BOARD_FRAME_HIDE_HELD] != 0.0) 0f else 1f
    overlay?.visibility = if (floating) View.VISIBLE else View.INVISIBLE
    overlayParent?.getLocationOnScreen(location)
    overlay?.translationX = (frame[ShadowListKitCore.BOARD_FRAME_X] - location[POINT_X] - cell.width / 2.0).toFloat()
    overlay?.translationY = (frame[ShadowListKitCore.BOARD_FRAME_Y] - location[POINT_Y] - cell.height / 2.0).toFloat()
    target.get()?.takeIf { floating }?.let {
      it.getLocationOnScreen(location)
      it.drag.updateForeign((frame[ShadowListKitCore.BOARD_FRAME_X] - location[POINT_X]).toFloat(),
        (frame[ShadowListKitCore.BOARD_FRAME_Y] - location[POINT_Y]).toFloat())
    }
    return floating
  }

  private fun hitList(view: View, x: Int, y: Int): ShadowListKitListView? {
    val bounds = Rect()
    if (view.visibility != View.VISIBLE || !view.getGlobalVisibleRect(bounds) || !bounds.contains(x, y)) return null
    if (view is ViewGroup) {
      for (index in view.childCount - 1 downTo 0) hitList(view.getChildAt(index), x, y)?.let { return it }
    }
    return (view as? ShadowListKitListView)?.takeIf { lists.contains(it) }
  }

  fun scroll(list: ShadowListKitListView, dtSeconds: Double): Boolean {
    if (home.get() !== list) return false
    var ancestor = list.parent
    while (ancestor is View) {
      if (ancestor is ShadowListKitListView && ancestor.horizontal && ancestor.scrollEnabled && ancestor.maxOffset > 0) {
        ancestor.getLocationOnScreen(location)
        val offset = ancestor.offset
        val delta = ShadowListKitCore.nativeDragBoardScrollDelta(handle, fingerWindowX.toDouble(),
          (fingerWindowX - location[POINT_X]).toDouble(), ancestor.width.toDouble(), ancestor.density.toDouble(), dtSeconds)
        val wanted = offset + delta + if (delta != 0.0) sidewaysCarry else 0.0
        val next = wanted.roundToInt().coerceIn(0, ancestor.maxOffset)
        sidewaysCarry = if (next > 0 && next < ancestor.maxOffset) wanted - next else 0.0
        if (next != offset) {
          ancestor.writeOffset(next, byUser = true)
          sidewaysScrollView = WeakReference(ancestor)
          refreshTouch(list)
          list.drag.update()
        }
        break
      }
      ancestor = ancestor.parent
    }
    if (frame[ShadowListKitCore.BOARD_FRAME_FLOATING] == 0.0) return false
    val destination = target.get() ?: return true
    if (!destination.scrollEnabled) return true
    destination.getLocationOnScreen(location)
    val touch = destination.windowAlongAt(fingerWindowX - location[POINT_X], fingerWindowY - location[POINT_Y]).toDouble()
    val offset = destination.offset.toDouble()
    val next = ShadowListKitCore.dragAutoScrollOffset(touch, destination.windowAlong.toDouble(), offset,
      destination.maxOffset.toDouble(), destination.density.toDouble(), dtSeconds)
    if (abs(next - offset) >= 1) destination.writeOffset(next.toInt(), byUser = true)
    return true
  }

  fun noteFinger(list: ShadowListKitListView, x: Float, y: Float) {
    if (home.get() !== list) return
    list.getLocationOnScreen(location)
    fingerWindowX = location[POINT_X] + x
    fingerWindowY = location[POINT_Y] + y
  }

  fun refreshTouch(list: ShadowListKitListView) {
    if (home.get() !== list) return
    list.getLocationOnScreen(location)
    list.drag.setTouch(fingerWindowX - location[POINT_X], fingerWindowY - location[POINT_Y])
  }

  fun returnOffset(list: ShadowListKitListView, cell: ShadowListKitListCell): DoubleArray {
    handoff.fill(0.0)
    if (home.get() === list) {
      cell.getLocationOnScreen(location)
      ShadowListKitCore.nativeDragBoardReturnOffset(handle, location[POINT_X] + cell.width / 2.0,
        location[POINT_Y] + cell.height / 2.0, now(), handoff)
    }
    return handoff
  }

  fun finish(list: ShadowListKitListView, commit: Boolean): Boolean {
    if (handle == 0L || home.get() !== list) return false
    val flight = ShadowListKitCore.nativeDragBoardFlight(handle)
    snapSideways()
    val destination = target.get()
    val foreign = destination?.core?.isDragForeign == true
    val destinationKey = if (foreign) destination!!.core.dragInsertionKey else ""
    val kind = if (commit) ShadowListKitCore.nativeDragBoardRelease(handle,
      if (foreign) destination!!.core.dragInsertionIndex else -1, destinationKey, destination?.keys?.size ?: 0, now(), release)
    else ShadowListKitCore.BOARD_CANCEL
    var landed: ShadowListKitListCell? = null
    if (kind == ShadowListKitCore.BOARD_DROP && destination != null) {
      val sourceIndex = list.core.heldIndex
      list.drag.releaseHeldForBoard()
      try {
        val accepted = moveItemToList(list, sourceIndex, destination,
          release[ShadowListKitCore.BOARD_RELEASE_INDEX].toInt(), destinationKey)
        if (!isCurrentFlight(list, flight)) return true
        ShadowListKitCore.nativeDragBoardLanded(handle, destination.dragListId.orEmpty(), accepted)
        landed = destination.mounted[heldKey]
      } catch (failure: Throwable) {
        endFlight()
        throw failure
      }
    }
    endForeign(destination)
    list.core.dragSetHeldAway(false)
    list.heldCell?.alpha = 1f
    val picture = overlay
    if (landed != null && picture != null && ShadowListKitCore.nativeDragBoardBeginOverlaySettle(handle)) {
      landingCell = WeakReference(landed)
      landed.alpha = 0f
      landed.getLocationOnScreen(location)
      val left = location[POINT_X]
      val top = location[POINT_Y]
      overlayParent?.getLocationOnScreen(location)
      picture.animate().translationX((left - location[POINT_X]).toFloat()).translationY((top - location[POINT_Y]).toFloat())
        .setDuration(ShadowListKitCore.DROP_DURATION_MS).withEndAction {
          if (handle != 0L && ShadowListKitCore.nativeDragBoardFlight(handle) == flight) {
            endFlight()
          }
        }.start()
    } else endFlight()
    return kind != ShadowListKitCore.BOARD_HOME
  }

  private fun endForeign(list: ShadowListKitListView?) {
    val core = list?.coreOrNull ?: return
    if (core.handle != 0L && core.isDragForeign) {
      core.dragEnd()
      list.drag.clearForeign()
    }
  }

  private fun isCurrentFlight(list: ShadowListKitListView, flight: Long): Boolean =
    handle != 0L && home.get() === list && ShadowListKitCore.nativeDragBoardFlight(handle) == flight

  /*
   * Touch and accessibility share this scoped application-owned data transaction.
   */
  fun moveItemToList(source: ShadowListKitListView, sourceIndex: Int, destination: ShadowListKitListView,
    destinationIndex: Int, destinationKey: String): Boolean {
    val key = source.keys.getOrNull(sourceIndex) ?: return false
    if (source.delegate?.canMoveItem(source, sourceIndex) == false) return false
    if (handle == 0L || source.dragBoard !== this || destination.dragBoard !== this) return false
    val sourceCore = source.core
    val destinationCore = destination.core
    val flight = ShadowListKitCore.nativeDragBoardFlight(handle)
    val previousHome = home.get()
    fun current(): Boolean = handle != 0L && source.dragBoard === this && destination.dragBoard === this &&
      home.get() === previousHome && ShadowListKitCore.nativeDragBoardFlight(handle) == flight
    sourceCore.setDropHint(ShadowListKitCore.DROP_REMOVED, key)
    destinationCore.setDropHint(ShadowListKitCore.DROP_INSERTED, key)
    try {
      source.delegate?.moveItemToList(source, sourceIndex, destination.dragListId.orEmpty(), destinationIndex, destinationKey)
      if (!current()) return false
      source.reloadData()
      if (!current()) return false
      destination.reloadData()
      if (!current()) return false
      source.layoutPass()
      if (!current()) return false
      destination.layoutPass()
      return current() && destination.keys.contains(key)
    } finally {
      if (sourceCore.handle != 0L) sourceCore.setDropHint(ShadowListKitCore.DROP_NONE, "")
      if (destinationCore.handle != 0L) destinationCore.setDropHint(ShadowListKitCore.DROP_NONE, "")
    }
  }

  fun moveTargets(source: ShadowListKitListView): List<ShadowListKitListView> {
    if (!source.reorderEnabled || source.isSectioned || source.editing) return emptyList()
    return lists.filter { it !== source && it.reorderEnabled && !it.isSectioned && !it.dragListId.isNullOrEmpty() &&
      it.isAttachedToWindow && it.isShown && it.windowToken == source.windowToken }
      .sortedWith(compareBy({ it.getLocationOnScreen(location); location[POINT_X] }, { it.getLocationOnScreen(location); location[POINT_Y] }))
  }

  fun performAccessibilityMove(source: ShadowListKitListView, sourceIndex: Int, destination: ShadowListKitListView): Boolean {
    if (handle == 0L || !ShadowListKitCore.nativeDragBoardIsIdle(handle) || !moveTargets(source).contains(destination) ||
      source.delegate?.canMoveItem(source, sourceIndex) == false) return false
    val accepted = moveItemToList(source, sourceIndex, destination, destination.keys.size, "")
    if (accepted) destination.announceForAccessibility(destination.context.getString(R.string.shadowlist_moved_to,
      destination.dragListLabel?.takeIf { it.isNotEmpty() } ?: destination.dragListId))
    return true
  }

  private fun endFlight() {
    endForeign(target.get())
    home.get()?.let {
      it.heldCell?.alpha = 1f
      it.coreOrNull?.takeIf { core -> core.handle != 0L }?.dragSetHeldAway(false)
    }
    landingCell.get()?.alpha = 1f
    landingCell.clear()
    overlay?.animate()?.cancel()
    overlay?.let { overlayParent?.overlay?.remove(it) }
    overlay = null
    overlayParent = null
    home.clear()
    target.clear()
    frame.fill(0.0)
    if (handle != 0L) ShadowListKitCore.nativeDragBoardEnd(handle)
  }

  private fun snapSideways() {
    val outer = sidewaysScrollView.get()
    sidewaysScrollView.clear()
    if (outer != null && outer.isAttachedToWindow && outer.snapToItem && outer.coreOrNull?.handle?.let { it != 0L } == true) {
      outer.gesture.animateTo(outer.snapTarget(outer.offset))
    }
  }


  companion object {
    const val POINT_X = 0
    const val POINT_Y = 1
    const val POINT_SLOTS = 2
    private val boards = HashMap<String, WeakReference<ShadowListKitDragBoard>>()
    private val reclaimer = ShadowListKitPeerReclaimer(ShadowListKitCore::nativeDragBoardDestroy)
    private fun now(): Double = SystemClock.uptimeMillis().toDouble()

    fun join(list: ShadowListKitListView) {
      val key = list.dragBoardId?.takeIf { it.isNotEmpty() } ?: return
      if (list.dragListId.isNullOrEmpty()) return
      boards.entries.removeAll { it.value.get() == null }
      val board = boards[key]?.get() ?: ShadowListKitDragBoard().also { boards[key] = WeakReference(it) }
      board.lists.add(list)
      list.dragBoard = board
    }
  }
}
