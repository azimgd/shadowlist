package com.shadowlist.utils

/*
 * A source attached to a completed reply.
 */
data class ShadowListUtilsAssistantSource(
  val key: String,
  val title: String,
  val url: String,
  val domain: String? = null,
)
