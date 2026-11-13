/*
 * A tool call at a character position in the reply. Status owns its output when applicable.
 */
public struct ShadowListUtilsAssistantToolCall: Hashable {
  public var key: String
  public var name: String
  public var input: String
  public var at: Int?
  public var status: ShadowListUtilsAssistantToolStatus

  public init(
    key: String, name: String, input: String, at: Int? = nil, status: ShadowListUtilsAssistantToolStatus
  ) {
    self.key = key
    self.name = name
    self.input = input
    self.at = at
    self.status = status
  }
}
