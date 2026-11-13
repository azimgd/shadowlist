import UIKit

/*
 * An image of a masonry card. width and height are the image's own size, which gives the card
 * its aspect. alt is what a screen reader says for it.
 */
public struct ShadowListUtilsMasonryImage: Hashable {
  public var url: String
  public var width: CGFloat
  public var height: CGFloat
  public var alt: String?

  public init(url: String, width: CGFloat, height: CGFloat, alt: String? = nil) {
    self.url = url
    self.width = width
    self.height = height
    self.alt = alt
  }
}

public struct ShadowListUtilsMasonryItem: Hashable {
  public var key: String
  public var image: ShadowListUtilsMasonryImage
  public var title: String?

  public init(key: String, image: ShadowListUtilsMasonryImage, title: String? = nil) {
    self.key = key
    self.image = image
    self.title = title
  }
}
