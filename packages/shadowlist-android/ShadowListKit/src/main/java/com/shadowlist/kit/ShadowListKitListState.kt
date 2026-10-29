package com.shadowlist.kit

import androidx.annotation.Keep

/*
 * What a list keeps across the cores it makes and drops: the row keys, the selected keys, the
 * batch performBatchUpdates collects and the content versions applyChanges compares, all in the
 * core's HeldKeys, ListSelection, BatchCollector and ContentVersions in C++. The list drops its
 * core on detach, and this lives as long as the list. A key change crosses once, here, and
 * reaches the live core from the native side. Every call runs on the UI thread.
 */
internal class ShadowListKitListState {
  companion object {
    init {
      System.loadLibrary("shadowlistkit")
    }

    /*
     * Frees the peer once its list is collected. A list has no call that ends it.
     */
    private val reclaimer = ShadowListKitPeerReclaimer(::nativeDestroy)

    /*
     * Outcomes of tap, the same as TapOutcome in the JNI.
     */
    const val TAP_NOTHING = 0
    const val TAP_SELECTED = 1
    const val TAP_TOGGLED_OFF = 2

    /*
     * What commitKeys does besides taking the keys, the same as CommitFlag in the JNI.
     */
    const val COMMIT_DIFF = 1
    const val COMMIT_RECORD_VERSIONS = 2
    const val COMMIT_UPDATE_VERSIONS = 4

    @JvmStatic private external fun nativeCreate(): Long
    @JvmStatic private external fun nativeDestroy(handle: Long)
    @JvmStatic private external fun nativeSetMultipleSelection(handle: Long, multiple: Boolean)
    @JvmStatic private external fun nativeSelectedCount(handle: Long): Int
    @JvmStatic private external fun nativeIsSelected(handle: Long, key: String): Boolean
    @JvmStatic private external fun nativeSelect(handle: Long, key: String): Array<String>
    @JvmStatic private external fun nativeDeselect(handle: Long, key: String): Boolean
    @JvmStatic private external fun nativeClearSelection(handle: Long): Array<String>
    @JvmStatic private external fun nativeTap(handle: Long, key: String, shouldSelect: ShouldSelect, out: IntArray): Array<String>?
    @JvmStatic private external fun nativeSelectedRows(handle: Long): IntArray
    @JvmStatic private external fun nativeBeginBatch(handle: Long)
    @JvmStatic private external fun nativeEndBatch(handle: Long): Boolean
    @JvmStatic private external fun nativeIsBatching(handle: Long): Boolean
    @JvmStatic private external fun nativeBatchDeleted(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeBatchInserted(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeBatchMoved(handle: Long, from: Int, to: Int)
    @JvmStatic private external fun nativeBatchReloaded(handle: Long, indices: IntArray)
    @JvmStatic private external fun nativeBatchNeedsReload(handle: Long)
    @JvmStatic private external fun nativeTakeBatch(handle: Long): IntArray?
    @JvmStatic private external fun nativePlanTakenBatch(handle: Long, previousCount: Int, nextCount: Int): IntArray?
    @JvmStatic private external fun nativeCommitKeys(
      handle: Long, core: Long, rowStart: Int, rowEnd: Int, rowPacked: CharArray, rowEnds: IntArray, sectioned: Boolean,
      itemStart: Int, itemEnd: Int, itemPacked: CharArray, itemEnds: IntArray, versions: LongArray?, flags: Int): IntArray
    @JvmStatic private external fun nativeInsertRows(handle: Long, core: Long, indices: IntArray, packed: CharArray, ends: IntArray)
    @JvmStatic private external fun nativeDeleteRows(handle: Long, core: Long, indices: IntArray)
    @JvmStatic private external fun nativeSeedCore(handle: Long, core: Long)
  }

  /*
   * Asked by tap before a row gets selected. Only JNI calls it. Keep stops R8 in a minified app
   * from removing or renaming it.
   */
  @Keep
  fun interface ShouldSelect {
    fun shouldSelect(): Boolean
  }

  private val handle: Long = nativeCreate().also { reclaimer.register(this, it) }
  private val tapOut = IntArray(1)
  private val rowPacker = ShadowListKitKeyPacker()
  private val itemPacker = ShadowListKitKeyPacker()

  // region Selection

  /*
   * Whether any row is selected, read again after every call that changes the selection. A
   * list with nothing selected answers isSelected without a JNI call.
   */
  var hasSelection = false
    private set

  private fun <T> changingSelection(change: () -> T): T =
    change().also { hasSelection = nativeSelectedCount(handle) > 0 }

  fun setMultipleSelection(multiple: Boolean) = changingSelection { nativeSetMultipleSelection(handle, multiple) }
  val selectedCount: Int get() = if (hasSelection) nativeSelectedCount(handle) else 0
  fun isSelected(key: String): Boolean = hasSelection && nativeIsSelected(handle, key)

  /*
   * Select a row. Returns the rows this deselected, the one before under single selection.
   */
  fun select(key: String): Array<String> = changingSelection { nativeSelect(handle, key) }
  fun deselect(key: String): Boolean = changingSelection { nativeDeselect(handle, key) }

  /*
   * Deselect every row. Returns them.
   */
  fun clearSelection(): Array<String> = changingSelection { nativeClearSelection(handle) }

  /*
   * What a tap did: a TAP_* outcome and the rows it deselected.
   */
  class Tap(val outcome: Int, val deselected: Array<String>)

  /*
   * A tap on a row by the core's ListSelection.tap. shouldSelect is asked only before the row
   * gets selected, never before a multiple selection toggles it off.
   */
  fun tap(key: String, shouldSelect: ShouldSelect): Tap {
    val deselected = changingSelection { nativeTap(handle, key, shouldSelect, tapOut) } ?: return Tap(TAP_NOTHING, emptyArray())
    return Tap(tapOut[0], deselected)
  }

  /*
   * The rows held that are selected, low to high.
   */
  fun selectedRows(): IntArray = if (hasSelection) nativeSelectedRows(handle) else IntArray(0)

  // endregion

  // region Batch

  /*
   * performBatchUpdates' block starts and returns. endBatch returns whether it was the
   * outermost, after which the batch is taken.
   */
  fun beginBatch() = nativeBeginBatch(handle)
  fun endBatch(): Boolean = nativeEndBatch(handle)
  val isBatching: Boolean get() = nativeIsBatching(handle)

  fun batchDeleted(indices: IntArray) = nativeBatchDeleted(handle, indices)
  fun batchInserted(indices: IntArray) = nativeBatchInserted(handle, indices)
  fun batchMoved(from: Int, to: Int) = nativeBatchMoved(handle, from, to)
  fun batchReloaded(indices: IntArray) = nativeBatchReloaded(handle, indices)
  fun batchNeedsReload() = nativeBatchNeedsReload(handle)

  /*
   * Take the collected batch: null when it is empty, otherwise whether it needs a reload (1 or
   * 0) then the reloaded items. planTakenBatch plans it once.
   */
  fun takeBatch(): IntArray? = nativeTakeBatch(handle)

  /*
   * The source of every row after the taken batch, a previous index or -1 for an insert, or
   * null when the batch does not add up. See the core's planBatch.
   */
  fun planTakenBatch(previousCount: Int, nextCount: Int): IntArray? = nativePlanTakenBatch(handle, previousCount, nextCount)

  // endregion

  // region Keys

  /*
   * Take the next row keys, with the items of a list with sections or null without. Only the
   * keys between rowStart and rowEnd unchanged keys at the ends cross, and the same for the
   * items. The native side splices the keys it holds, hands the splice to core when it is live,
   * keeps or compares versions, one per item, and drops the selected rows that left. Returns
   * the COMMIT_DIFF diff of the items, packed as the delete count and previous indices, the
   * insert count and next indices, the move count and from, to pairs, then the count and items
   * whose version changed under COMMIT_UPDATE_VERSIONS.
   */
  fun commitKeys(
    core: ShadowListKitCore?, rows: List<String>, rowStart: Int, rowEnd: Int,
    items: List<String>?, itemStart: Int, itemEnd: Int, versions: LongArray?, flags: Int,
  ): IntArray {
    val rowEnds = rowPacker.pack(rows, rowStart, rows.size - rowEnd)
    val itemEnds = if (items != null) itemPacker.pack(items, itemStart, items.size - itemEnd) else IntArray(0)
    return droppingSelection {
      nativeCommitKeys(handle, core?.handle ?: 0L, rowStart, rowEnd, rowPacker.chars, rowEnds, items != null,
        itemStart, itemEnd, itemPacker.chars, itemEnds, versions, flags)
    }
  }

  /*
   * Rows inserted at the sorted positions of the next keys of a list without sections.
   */
  fun insertRows(core: ShadowListKitCore?, indices: IntArray, keys: Array<String>) {
    val ends = rowPacker.pack(keys.asList())
    nativeInsertRows(handle, core?.handle ?: 0L, indices, rowPacker.chars, ends)
  }

  /*
   * Rows deleted at the sorted positions of the previous keys of a list without sections.
   */
  fun deleteRows(core: ShadowListKitCore?, indices: IntArray) =
    droppingSelection { nativeDeleteRows(handle, core?.handle ?: 0L, indices) }

  /*
   * A key change only drops selected rows. A list with nothing selected skips the count.
   */
  private fun <T> droppingSelection(change: () -> T): T =
    change().also { if (hasSelection) hasSelection = nativeSelectedCount(handle) > 0 }

  /*
   * Give a new core every key held.
   */
  fun seedCore(core: ShadowListKitCore) = nativeSeedCore(handle, core.handle)

  // endregion
}
