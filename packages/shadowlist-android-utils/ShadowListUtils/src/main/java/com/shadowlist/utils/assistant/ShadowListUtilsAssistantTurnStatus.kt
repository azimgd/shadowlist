package com.shadowlist.utils

/*
 * The RN turn states. Done and failed own only their applicable result fields.
 */
sealed interface ShadowListUtilsAssistantTurnStatus {
  data object Streaming : ShadowListUtilsAssistantTurnStatus
  data class Done(val sources: List<ShadowListUtilsAssistantSource> = emptyList(), val followUps: List<String> = emptyList()) : ShadowListUtilsAssistantTurnStatus
  data object Stopped : ShadowListUtilsAssistantTurnStatus
  data class Failed(val error: String? = null) : ShadowListUtilsAssistantTurnStatus
}
