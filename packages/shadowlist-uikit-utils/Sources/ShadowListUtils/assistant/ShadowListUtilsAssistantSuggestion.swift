/*
 * A suggested prompt. Selection returns this application-owned value unchanged.
 */
public struct ShadowListUtilsAssistantSuggestion: Hashable {
  public var title: String
  public var prompt: String
  public init(title: String, prompt: String) { self.title = title; self.prompt = prompt }
}
