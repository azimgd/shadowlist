import UIKit

/*
 * Who wrote a message. Without an avatarColor the avatar takes the palette color of the name.
 */
public struct ShadowListUtilsChatAuthor: Hashable {
  public var key: String
  public var name: String
  public var avatarUrl: String?
  public var avatarColor: UIColor?

  public init(key: String, name: String, avatarUrl: String? = nil, avatarColor: UIColor? = nil) {
    self.key = key
    self.name = name
    self.avatarUrl = avatarUrl
    self.avatarColor = avatarColor
  }
}

/*
 * Delivery state of the reader's own message. One value instead of a flag per state: a message
 * cannot be both sending and failed. Messages from others leave it unset.
 */
public enum ShadowListUtilsChatMessageStatus: Hashable {
  case sending
  case sent
  case delivered
  case read
  case failed
}

/*
 * A chat message: text, images, or both. isOwn puts it on the trailing side. createdAtMs is in
 * milliseconds since 1970. A sending message is dimmed and a failed one shows a line under it.
 */
public struct ShadowListUtilsChatMessage: Hashable {
  public var key: String
  public var author: ShadowListUtilsChatAuthor
  public var isOwn: Bool
  public var text: String?
  public var images: [String]
  public var createdAtMs: Double?
  public var status: ShadowListUtilsChatMessageStatus?

  public init(
    key: String,
    author: ShadowListUtilsChatAuthor,
    isOwn: Bool,
    text: String? = nil,
    images: [String] = [],
    createdAtMs: Double? = nil,
    status: ShadowListUtilsChatMessageStatus? = nil
  ) {
    self.key = key
    self.author = author
    self.isOwn = isOwn
    self.text = text
    self.images = images
    self.createdAtMs = createdAtMs
    self.status = status
  }
}
