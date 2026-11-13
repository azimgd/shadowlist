package com.shadowlist.utils

internal data class ShadowListUtilsAssistantHandlers(
  val onCopy: ((String) -> Unit)?,
  val onCopyCode: ((String) -> Unit)?,
  val onShare: ((String) -> Unit)?,
  val onRegenerate: ((String) -> Unit)?,
  val onRetry: ((String) -> Unit)?,
  val onSelectVariant: ((String, Int) -> Unit)?,
  val onFeedback: ((String, ShadowListUtilsAssistantFeedback?) -> Unit)?,
  val onFollowUp: ((String) -> Unit)?,
  val onOpenLink: ((String) -> Unit)?,
  val onEdit: ((String) -> Unit)?,
)
