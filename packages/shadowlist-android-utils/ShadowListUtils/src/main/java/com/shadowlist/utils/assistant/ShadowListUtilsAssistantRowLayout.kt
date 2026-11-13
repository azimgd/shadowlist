package com.shadowlist.utils

internal class ShadowListUtilsAssistantRowLayout(widthPx: Int, heightPx: Int, val content: Content) : ShadowListUtilsRowLayout(widthPx, heightPx) {
  sealed interface Content {
    data class User(val layout: ShadowListUtilsAssistantUserMessage.Layout) : Content
    data class Assistant(val layout: ShadowListUtilsAssistantReplyMessage.Layout) : Content
    data object End : Content
  }
}
