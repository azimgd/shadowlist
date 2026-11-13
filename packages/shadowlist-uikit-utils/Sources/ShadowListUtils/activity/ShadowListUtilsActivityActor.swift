import UIKit

/*
 * The person whose activity the row shows. Without avatarColor the name picks its palette color.
 */
public struct ShadowListUtilsActivityActor: Hashable {
  public var name: String
  public var avatarUrl: String?
  public var avatarColor: UIColor?

  public init(name: String, avatarUrl: String? = nil, avatarColor: UIColor? = nil) {
    self.name = name
    self.avatarUrl = avatarUrl
    self.avatarColor = avatarColor
  }
}
