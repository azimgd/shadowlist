package com.shadowlist.utils

/*
 * Read-only Kotlin collection interfaces can wrap an application's mutable collections. Copy
 * their containers at the UI boundary before a row is sent to background layout prefetch.
 */
internal fun ShadowListUtilsAssistantTurn.snapshot(): ShadowListUtilsAssistantTurn = copy(
  toolCalls = toolCalls.toList(),
  status = when (val state = status) {
    is ShadowListUtilsAssistantTurnStatus.Done -> state.copy(sources = state.sources.toList(), followUps = state.followUps.toList())
    else -> state
  },
)

internal fun ShadowListUtilsAssistantPrompt.snapshot(): ShadowListUtilsAssistantPrompt = copy(attachments = attachments.toList())

internal fun ShadowListUtilsAssistantReply.snapshot(): ShadowListUtilsAssistantReply = copy(variants = variants.map { it.snapshot() })

internal fun ShadowListUtilsAssistantMessage.snapshot(): ShadowListUtilsAssistantMessage = when (this) {
  is ShadowListUtilsAssistantMessage.User -> copy(prompt = prompt.snapshot())
  is ShadowListUtilsAssistantMessage.Assistant -> copy(reply = reply.snapshot())
  is ShadowListUtilsAssistantMessage.End -> this
}
