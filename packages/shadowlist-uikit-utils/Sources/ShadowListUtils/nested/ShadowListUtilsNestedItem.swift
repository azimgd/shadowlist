import UIKit

/*
 * A card of a nested row: an image with a title of two lines under it.
 */
public struct ShadowListUtilsNestedCardItem: Hashable {
  public var key: String
  public var title: String
  public var imageUrl: String
  public var imageAlt: String?

  public init(key: String, title: String, imageUrl: String, imageAlt: String? = nil) {
    self.key = key
    self.title = title
    self.imageUrl = imageUrl
    self.imageAlt = imageAlt
  }
}

/*
 * A titled row that holds a horizontal list of cards.
 */
public struct ShadowListUtilsNestedItem: Hashable {
  public var key: String
  public var title: String
  public var cards: [ShadowListUtilsNestedCardItem]

  public init(key: String, title: String, cards: [ShadowListUtilsNestedCardItem]) {
    self.key = key
    self.title = title
    self.cards = cards
  }
}
