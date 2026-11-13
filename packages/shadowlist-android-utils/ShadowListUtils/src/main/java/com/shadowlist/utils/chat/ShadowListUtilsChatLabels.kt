package com.shadowlist.utils

/*
 * image is what a screen reader says for an image. placeholder and send are the composer's.
 * sending is the hint of a message on its way, failed the line under a message that did not
 * arrive, and retryHint what pressing that line does.
 */
data class ShadowListUtilsChatLabels(
  val image: String = "Image",
  val placeholder: String = "Message",
  val send: String = "Send",
  val sending: String = "Sending",
  val failed: String = "Not delivered. Tap to retry.",
  val retryHint: String = "Sends the message again",
)
