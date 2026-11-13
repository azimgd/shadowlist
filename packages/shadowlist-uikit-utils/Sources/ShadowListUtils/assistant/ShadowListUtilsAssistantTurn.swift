/*
 * One reply variant. Status owns the fields that only exist after completion or failure.
 */
public struct ShadowListUtilsAssistantTurn: Hashable {
  public var content: String
  public var thinking: String?
  public var thinkingMs: Double?
  public var toolCalls: [ShadowListUtilsAssistantToolCall]
  public var status: ShadowListUtilsAssistantTurnStatus

  public init(
    content: String = "", thinking: String? = nil, thinkingMs: Double? = nil, toolCalls: [ShadowListUtilsAssistantToolCall] = [], status: ShadowListUtilsAssistantTurnStatus = .streaming
  ) {
    self.content = content
    self.thinking = thinking
    self.thinkingMs = thinkingMs
    self.toolCalls = toolCalls
    self.status = status
  }
}
