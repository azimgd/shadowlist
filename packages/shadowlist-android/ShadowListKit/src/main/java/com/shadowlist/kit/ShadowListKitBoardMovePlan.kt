package com.shadowlist.kit

/*
 * The shared board's resolved source and destination. Missing indices are -1.
 * An empty plan changes nothing. Hosts remove matching keys and insert at destinationIndex.
 */
class ShadowListKitBoardMovePlan private constructor(
  val sourceColumn: Int,
  val sourceIndex: Int,
  val destinationColumn: Int,
  val destinationIndex: Int,
) {
  val isEmpty: Boolean get() = destinationColumn < 0

  companion object {
    init { System.loadLibrary("shadowlistkit") }
    fun plan(columns: List<List<String>>, itemKey: String, destinationColumn: Int, destinationKey: String, destinationIndex: Int): ShadowListKitBoardMovePlan {
      val values = nativePlan(columns.map { it.toTypedArray() }.toTypedArray(), itemKey, destinationColumn, destinationKey, destinationIndex)
      return ShadowListKitBoardMovePlan(values[PLAN_SOURCE_COLUMN], values[PLAN_SOURCE_INDEX], values[PLAN_DESTINATION_COLUMN], values[PLAN_DESTINATION_INDEX])
    }
    @JvmStatic private external fun nativePlan(columns: Array<Array<String>>, itemKey: String, destinationColumn: Int, destinationKey: String, destinationIndex: Int): IntArray
    private const val PLAN_SOURCE_COLUMN = 0
    private const val PLAN_SOURCE_INDEX = 1
    private const val PLAN_DESTINATION_COLUMN = 2
    private const val PLAN_DESTINATION_INDEX = 3
  }
}
