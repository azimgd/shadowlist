package com.shadowlist.utils

/*
 * A card of a nested row: an image with a title of two lines under it.
 */
data class ShadowListUtilsNestedCardItem(
  val key: String,
  val title: String,
  val imageUrl: String,
  val imageAlt: String? = null,
)

/*
 * A titled row that holds a horizontal list of cards.
 */
data class ShadowListUtilsNestedItem(
  val key: String,
  val title: String,
  val cards: List<ShadowListUtilsNestedCardItem>,
)
