package com.shadowlist.kit

/*
 * Visible tree rows owned by the shared TreeSource. The host encodes its own objects once;
 * the core walks expansion state and splices branches. All calls run on the UI thread.
 */
class ShadowListKitTreeRows {
  private var handle = nativeCreate().also { reclaimer.register(this, it) }

  fun reset(rows: List<ShadowListKitTreeRow>) {
    nativeReset(
      peer(),
      rows.map { it.key }.toTypedArray(),
      rows.map { it.depth }.toIntArray(),
      rows.map {
        (if (it.hasChildren) TREE_ROW_HAS_CHILDREN else 0) or (if (it.expanded) TREE_ROW_EXPANDED else 0)
      }.toIntArray()
    )
  }

  fun setExpandedKeys(keys: Collection<String>) {
    nativeSetExpandedKeys(peer(), keys.toTypedArray())
  }
  fun toggleKey(key: String) {
    nativeToggle(peer(), key)
  }
  fun expandRow(index: Int) {
    nativeExpand(peer(), index)
  }
  fun collapseRow(index: Int) {
    nativeCollapse(peer(), index)
  }

  val rows: List<ShadowListKitTreeRow>
    get() {
      val keys = nativeKeys(peer())
      val metadata = nativeMetadata(peer())
      return keys.mapIndexed { index, key ->
        val at = index * ROW_SLOTS
        val flags = metadata[at + ROW_FLAGS]
        ShadowListKitTreeRow(
          key,
          metadata[at + ROW_DEPTH],
          flags and TREE_ROW_HAS_CHILDREN != 0,
          flags and TREE_ROW_EXPANDED != 0
        )
      }
    }

  val expandedKeys: List<String> get() = nativeExpandedKeys(peer()).toList()

  fun destroy() {
    if (handle == 0L) return
    nativeDestroy(handle)
    reclaimer.release(handle)
    handle = 0L
  }

  private fun peer(): Long {
    check(handle != 0L) { "Tree rows were destroyed" }
    return handle
  }

  private external fun nativeCreate(): Long
  private external fun nativeReset(handle: Long, keys: Array<String>, depths: IntArray, flags: IntArray)
  private external fun nativeSetExpandedKeys(handle: Long, keys: Array<String>)
  private external fun nativeToggle(handle: Long, key: String)
  private external fun nativeExpand(handle: Long, index: Int)
  private external fun nativeCollapse(handle: Long, index: Int)
  private external fun nativeKeys(handle: Long): Array<String>
  private external fun nativeMetadata(handle: Long): IntArray
  private external fun nativeExpandedKeys(handle: Long): Array<String>

  private companion object {
    init {
      System.loadLibrary("shadowlistkit")
    }
    val reclaimer = ShadowListKitPeerReclaimer(::nativeDestroy)

    @JvmStatic external fun nativeDestroy(handle: Long)
    const val TREE_ROW_HAS_CHILDREN = 1
    const val TREE_ROW_EXPANDED = 2
    const val ROW_DEPTH = 0
    const val ROW_FLAGS = 1
    const val ROW_SLOTS = 2
  }
}
