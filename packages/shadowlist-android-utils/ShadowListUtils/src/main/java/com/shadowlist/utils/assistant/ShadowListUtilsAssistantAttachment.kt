package com.shadowlist.utils

/*
 * An image or file supplied by the application.
 */
data class ShadowListUtilsAssistantAttachment(
  val key: String,
  val kind: ShadowListUtilsAssistantAttachment.Kind,
  val name: String,
  val detail: String? = null,
  val uri: String? = null,
  val color: Int? = null,
) {
  enum class Kind { IMAGE, FILE }
}
