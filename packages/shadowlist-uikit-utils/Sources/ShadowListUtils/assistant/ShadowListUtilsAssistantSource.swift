/*
 * A source attached to a completed reply.
 */
public struct ShadowListUtilsAssistantSource: Hashable {
  public var key: String
  public var title: String
  public var url: String
  public var domain: String?

  public init(
    key: String, title: String, url: String, domain: String? = nil
  ) {
    self.key = key
    self.title = title
    self.url = url
    self.domain = domain
  }
}
