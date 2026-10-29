package com.shadowlist.kit

import androidx.annotation.Keep

/*
 * What a list keeps across the cores it makes and drops: the selected keys, the batch
 * performBatchUpdates collects and the content versions applyChanges compares, all in the core's
 * ListSelection, BatchCollector and ContentVersions in C++. The list drops its core on detach,
 * and this lives as long as the list. Every call runs on the UI thread.
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

    @JvmStatic private external fun nativeCreate(): Long
    @JvmStatic private external fun nativeDestroy(handle: Long)
    @JvmStatic private external fun nativeSetMultipleSelection(handle: Long, multiple: Boolean)
    @JvmStatic private external fun nativeSelectedCount(handle: Long): Int
    @JvmStatic private external fun nativeIsSelected(handle: Long, key: String): Boolean
    @JvmStatic private external fun nativeSelect(handle: Long, key: String): Array<String>
    @JvmStatic private external fun nativeDeselect(handle: Long, key: String): Boolean
    @JvmStatic private external fun nativeClearSelection(handle: Long): Array<String>
    @JvmStatic private external fun nativeTap(handle: Long, key: String, shouldSelect: ShouldSelect, out: IntArray): Array<String>?
    @JvmStatic private external fun nativeRetainSelection(handle: Long, packed: CharArray, ends: IntArray)
    @JvmStatic private external fun nativeSelectedRows(handle: Long, packed: CharArray, ends: IntArray): IntArray
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
    @JvmStatic private external fun nativeRecordContentVersions(handle: Long, packed: CharArray, ends: IntArray, versions: LongArray)
    @JvmStatic private external fun nativeUpdateContentVersions(
      handle: Long, packed: CharArray, ends: IntArray, versions: LongArray): IntArray
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
  private val packer = ShadowListKitKeyPacker()

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
   * The data changed. Selected rows whose key is gone drop out.
   */
  fun retainSelection(keys: List<String>) {
    if (!hasSelection) return
    val ends = packer.pack(keys)
    changingSelection { nativeRetainSelection(handle, packer.chars, ends) }
  }

  /*
   * The rows of keys that are selected, low to high.
   */
  fun selectedRows(keys: List<String>): IntArray {
    if (!hasSelection) return IntArray(0)
    val ends = packer.pack(keys)
    return nativeSelectedRows(handle, packer.chars, ends)
  }

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

  // region Content versions

  /*
   * Forget every version and keep these, one per item key.
   */
  fun recordContentVersions(itemKeys: List<String>, versions: LongArray) {
    val ends = packer.pack(itemKeys)
    nativeRecordContentVersions(handle, packer.chars, ends, versions)
  }

  /*
   * The items whose version changed under the same key, low to high. The new versions are kept.
   */
  fun updateContentVersions(itemKeys: List<String>, versions: LongArray): IntArray {
    val ends = packer.pack(itemKeys)
    return nativeUpdateContentVersions(handle, packer.chars, ends, versions)
  }

  // endregion
}
