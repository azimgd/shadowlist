/*
 * The application-owned reply variants and selected variant.
 */
public struct ShadowListUtilsAssistantReply: Hashable {
  public var key: String
  public var variants: [ShadowListUtilsAssistantTurn]
  public var variantIndex: Int
  public var model: String?
  public var feedback: ShadowListUtilsAssistantFeedback?

  public init(
    key: String, variants: [ShadowListUtilsAssistantTurn] = [], variantIndex: Int = 0, model: String? = nil, feedback: ShadowListUtilsAssistantFeedback? = nil
  ) {
    self.key = key
    self.variants = variants
    self.variantIndex = variantIndex
    self.model = model
    self.feedback = feedback
  }
}
