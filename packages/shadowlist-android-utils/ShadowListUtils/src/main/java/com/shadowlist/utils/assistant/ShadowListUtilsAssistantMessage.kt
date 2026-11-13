package com.shadowlist.utils

/*
 * The transcript's existing three roles. End is a stable trailing marker, never a message bubble.
 */
sealed interface ShadowListUtilsAssistantMessage {
  val key: String
  data class User(val prompt: ShadowListUtilsAssistantPrompt) : ShadowListUtilsAssistantMessage {
    override val key: String get() = prompt.key
  }
  data class Assistant(val reply: ShadowListUtilsAssistantReply) : ShadowListUtilsAssistantMessage {
    override val key: String get() = reply.key
  }
  data class End(override val key: String) : ShadowListUtilsAssistantMessage
  companion object { val endMarker = End("assistant-end") }
}
