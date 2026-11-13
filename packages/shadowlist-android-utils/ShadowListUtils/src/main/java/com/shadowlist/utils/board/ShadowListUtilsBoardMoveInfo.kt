package com.shadowlist.utils

/*
 * A request to move item before destinationKey, or to the end for an empty key.
 * destinationIndex is the fallback when that key no longer exists.
 */
data class ShadowListUtilsBoardMoveInfo<Item>(val item: Item, val sourceColumnKey: String, val destinationColumnKey: String,
                                           val destinationKey: String, val destinationIndex: Int)
