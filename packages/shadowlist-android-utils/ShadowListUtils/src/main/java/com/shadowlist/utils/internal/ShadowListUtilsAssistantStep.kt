package com.shadowlist.utils

/*
 * Typed copies of the shared AssistantStep variant. Offsets are integer UTF-16 positions.
 */
internal sealed interface ShadowListUtilsAssistantStep {
  data class Text(val text: String, val at: Int) : ShadowListUtilsAssistantStep
  data class Calls(val callIndices: IntArray) : ShadowListUtilsAssistantStep
}
