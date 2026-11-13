/*
 * Strings of the existing Assistant views. Applications own localization.
 */
public struct ShadowListUtilsAssistantLabels {
  public var assistantName: String = "Assistant"
  public var copy: String = "Copy"
  public var copied: String = "Copied"
  public var copyCode: String = "Copy code"
  public var codeLanguageFallback: String = "code"
  public var share: String = "Share"
  public var regenerate: String = "Regenerate"
  public var retry: String = "Retry"
  public var goodResponse: String = "Good response"
  public var badResponse: String = "Bad response"
  public var previousVersion: String = "Previous version"
  public var nextVersion: String = "Next version"
  public var responseStopped: String = "Response stopped"
  public var responseFailed: String = "Something went wrong."
  public var edit: String = "Edit"
  public var showActions: String = "Show actions"
  public var showActionsHint: String = "Long press for copy and edit"
  public var thinking: String = "Thinking"
  public var toolInput: String = "Input"
  public var toolOutput: String = "Output"
  public var typing: String = "Assistant is typing"
  public var scrollToLatest: String = "Scroll to latest"
  public var placeholder: String = "Ask anything"
  public var send: String = "Send"
  public var stop: String = "Stop generating"
  public var addAttachment: String = "Add attachment"
  public var dictate: String = "Dictate"
  public var stopDictating: String = "Stop dictating"
  public var editingMessage: String = "Editing message"
  public var cancelEditing: String = "Cancel editing"
  public var thinkingToggle: String = "Think"
  public var thinkingToggleDescription: String = "Extended thinking"
  public var emptyTitle: String = "How can I help?"
  public var emptySubtitle: String = ""
  public var versionPosition: (Int, Int) -> String = { "\($0) / \($1)" }
  public var source: (Int, String) -> String = { "Source \($0): \($1)" }
  public var thoughtFor: (Int) -> String = { "Thought for \($0)s" }
  public var removeAttachment: (String) -> String = { "Remove \($0)" }
  public var model: (String) -> String = { "Model: \($0)" }
  public var toolStatus: (ShadowListUtilsAssistantToolStatus) -> String = {
    switch $0 {
    case .running: return "Running"
    case .done: return "Done"
    case .failed: return "Failed"
    case .stopped: return "Stopped"
    }
  }

  public init() {}
}
