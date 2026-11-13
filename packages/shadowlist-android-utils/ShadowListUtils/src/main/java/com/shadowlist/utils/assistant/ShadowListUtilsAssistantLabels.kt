package com.shadowlist.utils

/*
 * Strings of the existing Assistant views. Applications own localization.
 */
data class ShadowListUtilsAssistantLabels(
  val assistantName: String = "Assistant",
  val copy: String = "Copy",
  val copied: String = "Copied",
  val copyCode: String = "Copy code",
  val codeLanguageFallback: String = "code",
  val share: String = "Share",
  val regenerate: String = "Regenerate",
  val retry: String = "Retry",
  val goodResponse: String = "Good response",
  val badResponse: String = "Bad response",
  val previousVersion: String = "Previous version",
  val nextVersion: String = "Next version",
  val responseStopped: String = "Response stopped",
  val responseFailed: String = "Something went wrong.",
  val edit: String = "Edit",
  val showActions: String = "Show actions",
  val showActionsHint: String = "Long press for copy and edit",
  val thinking: String = "Thinking",
  val toolInput: String = "Input",
  val toolOutput: String = "Output",
  val typing: String = "Assistant is typing",
  val scrollToLatest: String = "Scroll to latest",
  val placeholder: String = "Ask anything",
  val send: String = "Send",
  val stop: String = "Stop generating",
  val addAttachment: String = "Add attachment",
  val dictate: String = "Dictate",
  val stopDictating: String = "Stop dictating",
  val editingMessage: String = "Editing message",
  val cancelEditing: String = "Cancel editing",
  val thinkingToggle: String = "Think",
  val thinkingToggleDescription: String = "Extended thinking",
  val emptyTitle: String = "How can I help?",
  val emptySubtitle: String = "",
  val versionPosition: (Int, Int) -> String = { position, count -> "$position / $count" },
  val source: (Int, String) -> String = { position, title -> "Source $position: $title" },
  val thoughtFor: (Int) -> String = { "Thought for ${it}s" },
  val removeAttachment: (String) -> String = { "Remove $it" },
  val model: (String) -> String = { "Model: $it" },
  val toolStatus: (ShadowListUtilsAssistantToolStatus) -> String = {
    when (it) {
      ShadowListUtilsAssistantToolStatus.Running -> "Running"
      is ShadowListUtilsAssistantToolStatus.Done -> "Done"
      is ShadowListUtilsAssistantToolStatus.Failed -> "Failed"
      ShadowListUtilsAssistantToolStatus.Stopped -> "Stopped"
    }
  },
)
