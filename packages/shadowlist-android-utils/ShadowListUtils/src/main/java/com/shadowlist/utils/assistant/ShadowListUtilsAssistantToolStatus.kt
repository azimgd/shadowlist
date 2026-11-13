package com.shadowlist.utils

/*
 * A stopped tool is not a failed tool. Only done and failed carry output.
 */
sealed interface ShadowListUtilsAssistantToolStatus {
  data object Running : ShadowListUtilsAssistantToolStatus
  data class Done(val output: String) : ShadowListUtilsAssistantToolStatus
  data class Failed(val output: String? = null) : ShadowListUtilsAssistantToolStatus
  data object Stopped : ShadowListUtilsAssistantToolStatus
}
