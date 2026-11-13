import UIKit

/*
 * One poll option. key stays the same when its vote count changes.
 */
public struct ShadowListUtilsPollOption: Hashable {
  public var key: String
  public var label: String
  public var votes: Double
  public var icon: UIImage?

  public init(key: String, label: String, votes: Double, icon: UIImage? = nil) {
    self.key = key
    self.label = label
    self.votes = votes
    self.icon = icon
  }
}
