package com.shadowlist.utils

/*
 * The application-owned reply variants and selected variant.
 */
data class ShadowListUtilsAssistantReply(
  val key: String,
  val variants: List<ShadowListUtilsAssistantTurn> = emptyList(),
  val variantIndex: Int = 0,
  val model: String? = null,
  val feedback: ShadowListUtilsAssistantFeedback? = null,
)
