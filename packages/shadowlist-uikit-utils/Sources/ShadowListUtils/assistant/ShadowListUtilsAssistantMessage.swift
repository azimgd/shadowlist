/*
 * The transcript's existing three roles. End is a stable trailing marker, never a message bubble.
 */
public enum ShadowListUtilsAssistantMessage: Hashable {
  case user(ShadowListUtilsAssistantPrompt)
  case assistant(ShadowListUtilsAssistantReply)
  case end(key: String)

  public static let endMarker: ShadowListUtilsAssistantMessage = .end(key: "assistant-end")

  public var key: String {
    switch self {
    case let .user(prompt): return prompt.key
    case let .assistant(reply): return reply.key
    case let .end(key): return key
    }
  }
}
