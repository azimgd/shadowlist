import UIKit

/*
 * A person in a list. Without an avatarColor the avatar takes the palette color of the name.
 */
public struct ShadowListUtilsContactItem: Hashable {
  public var key: String
  public var name: String
  public var subtitle: String?
  public var avatarUrl: String?
  public var avatarColor: UIColor?

  public init(key: String, name: String, subtitle: String? = nil, avatarUrl: String? = nil, avatarColor: UIColor? = nil) {
    self.key = key
    self.name = name
    self.subtitle = subtitle
    self.avatarUrl = avatarUrl
    self.avatarColor = avatarColor
  }

  /*
   * What a screen reader says for the contact.
   */
  var accessibilityLabel: String {
    subtitle.flatMap { $0.isEmpty ? nil : "\(name), \($0)" } ?? name
  }
}

/*
 * A group of contacts under one title. key is the section's identity.
 */
public struct ShadowListUtilsContactSection: Hashable {
  public var key: String
  public var title: String
  public var items: [ShadowListUtilsContactItem]

  public init(key: String, title: String, items: [ShadowListUtilsContactItem]) {
    self.key = key
    self.title = title
    self.items = items
  }
}
