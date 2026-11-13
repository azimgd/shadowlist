package com.shadowlist.utils

/*
 * An activity. createdAtMs is milliseconds since 1970. Only an unread item shows the unread dot.
 */
data class ShadowListUtilsActivityItem(
  val key: String,
  val actor: ShadowListUtilsActivityActor,
  val action: String,
  val text: String? = null,
  val createdAtMs: Double,
  val read: Boolean = true,
)
