package com.shadowlist.utils

data class ShadowListUtilsFeedAuthor(
  val name: String,
  val handle: String? = null,
  val avatarUrl: String? = null,
  val avatarColor: Int? = null,
)

/*
 * An image of a post. width and height are the image's own size when known. color is drawn
 * behind the image while it loads, and alone in a placeholder row.
 */
data class ShadowListUtilsFeedImage(
  val url: String,
  val width: Float? = null,
  val height: Float? = null,
  val alt: String? = null,
  val color: Int? = null,
)

/*
 * A post. createdAtMs is in milliseconds since 1970. A placeholder item stands in for a post
 * still loading: its strings are drawn as bars and its images as colored frames of their size,
 * which makes the row the height the real post will be.
 */
data class ShadowListUtilsFeedItem(
  val key: String,
  val author: ShadowListUtilsFeedAuthor,
  val text: String? = null,
  val images: List<ShadowListUtilsFeedImage> = emptyList(),
  val createdAtMs: Double? = null,
  val placeholder: Boolean = false,
)
