package com.shadowlist.utils

/*
 * A user message and its attachments.
 */
data class ShadowListUtilsAssistantPrompt(
  val key: String,
  val text: String,
  val attachments: List<ShadowListUtilsAssistantAttachment> = emptyList(),
)
