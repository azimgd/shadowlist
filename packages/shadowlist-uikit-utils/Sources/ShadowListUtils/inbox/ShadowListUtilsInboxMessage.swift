/*
 * A mail message. receivedAtMs is in milliseconds since 1970.
 */
public struct ShadowListUtilsInboxMessage: Hashable {
  public var key: String
  public var sender: String
  public var subject: String
  public var preview: String
  public var receivedAtMs: Double
  public var read: Bool
  public var flagged: Bool
  public var avatarUrl: String?

  public init(
    key: String,
    sender: String,
    subject: String,
    preview: String,
    receivedAtMs: Double,
    read: Bool,
    flagged: Bool,
    avatarUrl: String? = nil
  ) {
    self.key = key
    self.sender = sender
    self.subject = subject
    self.preview = preview
    self.receivedAtMs = receivedAtMs
    self.read = read
    self.flagged = flagged
    self.avatarUrl = avatarUrl
  }
}
