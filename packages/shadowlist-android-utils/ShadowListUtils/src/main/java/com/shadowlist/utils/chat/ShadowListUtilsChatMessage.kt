package com.shadowlist.utils

/*
 * Who wrote a message. Without an avatarColor the avatar takes the palette color of the name.
 */
data class ShadowListUtilsChatAuthor(
  val key: String,
  val name: String,
  val avatarUrl: String? = null,
  val avatarColor: Int? = null,
)

/*
 * Delivery state of the reader's own message. One value instead of a flag per state: a message
 * cannot be both sending and failed. Messages from others leave it unset.
 */
enum class ShadowListUtilsChatMessageStatus { SENDING, SENT, DELIVERED, READ, FAILED }

/*
 * A chat message: text, images, or both. isOwn puts it on the trailing side. createdAtMs is in
 * milliseconds since 1970. A sending message is dimmed and a failed one shows a line under it.
 */
data class ShadowListUtilsChatMessage(
  val key: String,
  val author: ShadowListUtilsChatAuthor,
  val isOwn: Boolean,
  val text: String? = null,
  val images: List<String> = emptyList(),
  val createdAtMs: Double? = null,
  val status: ShadowListUtilsChatMessageStatus? = null,
)
