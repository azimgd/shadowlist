package com.shadowlist.utils

import com.shadowlist.kit.ShadowListKitBoardMovePlan

/*
 * Apply the shared board move plan to application objects. The core resolves keys and indices;
 * the host only removes matching objects and inserts the supplied item at the resolved place.
 */
fun <Item> shadowListUtilsMoveBoardItem(columns: List<ShadowListUtilsBoardColumn<Item>>, move: ShadowListUtilsBoardMoveInfo<Item>,
                                     keyExtractor: (Item) -> String): List<ShadowListUtilsBoardColumn<Item>> {
  val destination = columns.indexOfFirst { it.key == move.destinationColumnKey }
  if (destination < 0) return columns
  val key = keyExtractor(move.item)
  val plan = ShadowListKitBoardMovePlan.plan(columns.map { column -> column.items.map(keyExtractor) }, key, destination, move.destinationKey, move.destinationIndex)
  if (plan.isEmpty) return columns
  return columns.mapIndexed { index, column ->
    val items = column.items.filterNot { keyExtractor(it) == key }.toMutableList()
    if (index == plan.destinationColumn) items.add(plan.destinationIndex, move.item)
    column.copy(items = items)
  }
}
