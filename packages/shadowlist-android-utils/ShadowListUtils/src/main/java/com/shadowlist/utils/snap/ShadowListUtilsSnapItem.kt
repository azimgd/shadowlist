package com.shadowlist.utils

/*
 * A card of a snap list: an image that fills the card with a title and a subtitle over its
 * bottom edge. color fills the card behind the image, the theme's elevated color when null.
 */
data class ShadowListUtilsSnapItem(
  val key: String,
  val title: String? = null,
  val subtitle: String? = null,
  val imageUrl: String? = null,
  val imageAlt: String? = null,
  val color: Int? = null,
)
