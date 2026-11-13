package com.shadowlist.utils

/*
 * An image of a masonry card. width and height are the image's own size, which gives the card
 * its aspect. alt is what a screen reader says for it.
 */
data class ShadowListUtilsMasonryImage(
  val url: String,
  val width: Float,
  val height: Float,
  val alt: String? = null,
)

data class ShadowListUtilsMasonryItem(
  val key: String,
  val image: ShadowListUtilsMasonryImage,
  val title: String? = null,
)
