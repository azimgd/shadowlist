package com.shadowlist.utils

/*
 * A mail message. receivedAtMs is in milliseconds since 1970.
 */
data class ShadowListUtilsInboxMessage(
  val key: String,
  val sender: String,
  val subject: String,
  val preview: String,
  val receivedAtMs: Double,
  val read: Boolean,
  val flagged: Boolean,
  val avatarUrl: String? = null,
)
