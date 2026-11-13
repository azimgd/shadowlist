import UIKit

public struct ShadowListUtilsFeedAuthor: Hashable {
  public var name: String
  public var handle: String?
  public var avatarUrl: String?
  public var avatarColor: UIColor?

  public init(name: String, handle: String? = nil, avatarUrl: String? = nil, avatarColor: UIColor? = nil) {
    self.name = name
    self.handle = handle
    self.avatarUrl = avatarUrl
    self.avatarColor = avatarColor
  }
}

/*
 * An image of a post. width and height are the image's own size when known. color is drawn
 * behind the image while it loads, and alone in a placeholder row.
 */
public struct ShadowListUtilsFeedImage: Hashable {
  public var url: String
  public var width: CGFloat?
  public var height: CGFloat?
  public var alt: String?
  public var color: UIColor?

  public init(url: String, width: CGFloat? = nil, height: CGFloat? = nil, alt: String? = nil, color: UIColor? = nil) {
    self.url = url
    self.width = width
    self.height = height
    self.alt = alt
    self.color = color
  }
}

/*
 * A post. createdAtMs is in milliseconds since 1970. A placeholder item stands in for a post
 * still loading: its strings are drawn as bars and its images as colored frames of their size,
 * which makes the row the height the real post will be.
 */
public struct ShadowListUtilsFeedItem: Hashable {
  public var key: String
  public var author: ShadowListUtilsFeedAuthor
  public var text: String?
  public var images: [ShadowListUtilsFeedImage]
  public var createdAtMs: Double?
  public var placeholder: Bool

  public init(
    key: String,
    author: ShadowListUtilsFeedAuthor,
    text: String? = nil,
    images: [ShadowListUtilsFeedImage] = [],
    createdAtMs: Double? = nil,
    placeholder: Bool = false
  ) {
    self.key = key
    self.author = author
    self.text = text
    self.images = images
    self.createdAtMs = createdAtMs
    self.placeholder = placeholder
  }
}
