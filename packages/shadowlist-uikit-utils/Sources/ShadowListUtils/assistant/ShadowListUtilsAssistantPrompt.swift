/*
 * A user message and its attachments.
 */
public struct ShadowListUtilsAssistantPrompt: Hashable {
  public var key: String
  public var text: String
  public var attachments: [ShadowListUtilsAssistantAttachment]

  public init(
    key: String, text: String, attachments: [ShadowListUtilsAssistantAttachment] = []
  ) {
    self.key = key
    self.text = text
    self.attachments = attachments
  }
}
