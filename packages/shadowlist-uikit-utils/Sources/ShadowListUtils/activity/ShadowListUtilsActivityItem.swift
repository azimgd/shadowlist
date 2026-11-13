/*
 * An activity. createdAtMs is milliseconds since 1970. Only an unread item shows the unread dot.
 */
public struct ShadowListUtilsActivityItem: Hashable {
  public var key: String
  public var actor: ShadowListUtilsActivityActor
  public var action: String
  public var text: String?
  public var createdAtMs: Double
  public var read: Bool

  public init(
    key: String, actor: ShadowListUtilsActivityActor, action: String, text: String? = nil,
    createdAtMs: Double, read: Bool = true
  ) {
    self.key = key
    self.actor = actor
    self.action = action
    self.text = text
    self.createdAtMs = createdAtMs
    self.read = read
  }
}
