package com.shadowlist.utils

/*
 * A tile of a reorder grid: a colored swatch with an optional label on it and a title under it.
 * Without a color the swatch takes the palette color of the label, or of the title. aspectRatio
 * is the swatch's width over its height, 1 when null.
 */
data class ShadowListUtilsReorderTileItem(
  val key: String,
  val title: String,
  val label: String? = null,
  val color: Int? = null,
  val aspectRatio: Float? = null,
)
