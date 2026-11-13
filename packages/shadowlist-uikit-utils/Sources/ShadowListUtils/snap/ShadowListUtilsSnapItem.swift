import UIKit

/*
 * A card of a snap list: an image that fills the card with a title and a subtitle over its
 * bottom edge. color fills the card behind the image, the theme's elevated color when nil.
 */
public struct ShadowListUtilsSnapItem: Hashable {
  public var key: String
  public var title: String?
  public var subtitle: String?
  public var imageUrl: String?
  public var imageAlt: String?
  public var color: UIColor?

  public init(
    key: String,
    title: String? = nil,
    subtitle: String? = nil,
    imageUrl: String? = nil,
    imageAlt: String? = nil,
    color: UIColor? = nil
  ) {
    self.key = key
    self.title = title
    self.subtitle = subtitle
    self.imageUrl = imageUrl
    self.imageAlt = imageAlt
    self.color = color
  }
}
