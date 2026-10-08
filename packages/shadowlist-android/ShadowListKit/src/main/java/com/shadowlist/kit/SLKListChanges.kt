package com.shadowlist.kit

/*
 * What applyChanges found. Deleted and moved from are indices in the previous data, inserted,
 * moved to and reloaded indices in the new data.
 */
class SLKListChanges(
  val deletedIndices: IntArray,
  val insertedIndices: IntArray,
  val movedFromIndices: IntArray,
  val movedToIndices: IntArray,
  val reloadedIndices: IntArray,
) {
  val isEmpty: Boolean
    get() = deletedIndices.isEmpty() && insertedIndices.isEmpty() && movedFromIndices.isEmpty() && reloadedIndices.isEmpty()

  override fun toString(): String =
    "SLKListChanges(deleted=${deletedIndices.size}, inserted=${insertedIndices.size}, " +
      "moved=${movedFromIndices.size}, reloaded=${reloadedIndices.size})"
}
