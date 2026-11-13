package com.shadowlist.utils

/*
 * time words a post's age. image is what a screen reader says for an image without an alt text,
 * and loading for a placeholder row.
 */
data class ShadowListUtilsFeedLabels(
  val time: ShadowListUtilsRelativeTimeLabels = ShadowListUtilsRelativeTimeLabels(),
  val image: String = "Image",
  val loading: String = "Loading post",
)
