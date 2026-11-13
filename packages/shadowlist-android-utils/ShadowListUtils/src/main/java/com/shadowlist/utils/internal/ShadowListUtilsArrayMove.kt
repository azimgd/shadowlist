package com.shadowlist.utils

/*
 * The one array move the library uses.
 */
internal object ShadowListUtilsArrayMove {
  /*
   * The items with the one at sourceIndex taken out and put back at destinationIndex. An index
   * outside the list returns the items as they are.
   */
  fun <T> moved(items: List<T>, sourceIndex: Int, destinationIndex: Int): List<T> {
    if (sourceIndex !in items.indices || destinationIndex !in items.indices || sourceIndex == destinationIndex) return items
    val next = ArrayList(items)
    next.add(destinationIndex, next.removeAt(sourceIndex))
    return next
  }
}
