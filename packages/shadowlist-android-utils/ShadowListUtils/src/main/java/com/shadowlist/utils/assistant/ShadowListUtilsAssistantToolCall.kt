package com.shadowlist.utils

/*
 * A tool call at a character position in the reply. Status owns its output when applicable.
 */
data class ShadowListUtilsAssistantToolCall(
  val key: String,
  val name: String,
  val input: String,
  val at: Int? = null,
  val status: ShadowListUtilsAssistantToolStatus,
)
