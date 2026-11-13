package com.shadowlist.utils

/*
 * One reply variant. Status owns completion and failure fields.
 */
data class ShadowListUtilsAssistantTurn(
  val content: String = "",
  val thinking: String? = null,
  val thinkingMs: Double? = null,
  val toolCalls: List<ShadowListUtilsAssistantToolCall> = emptyList(),
  val status: ShadowListUtilsAssistantTurnStatus = ShadowListUtilsAssistantTurnStatus.Streaming,
)
