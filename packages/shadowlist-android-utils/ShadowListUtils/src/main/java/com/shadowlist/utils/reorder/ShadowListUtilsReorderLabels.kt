package com.shadowlist.utils

/*
 * dragHint is what a screen reader says a row is for. The others name the actions that move an
 * item by one place: up and down in a list, earlier and later in a grid.
 */
data class ShadowListUtilsReorderLabels(
  val dragHint: String = "Press and hold, then drag to reorder",
  val moveUp: String = "Move up",
  val moveDown: String = "Move down",
  val moveEarlier: String = "Move earlier",
  val moveLater: String = "Move later",
)
