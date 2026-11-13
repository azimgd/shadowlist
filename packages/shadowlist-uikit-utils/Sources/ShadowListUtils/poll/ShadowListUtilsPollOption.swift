/*
 * One poll option. key stays the same when its vote count changes.
 */
public struct ShadowListUtilsPollOption: Hashable {
  public var key: String
  public var label: String
  public var votes: Double

  public init(key: String, label: String, votes: Double) {
    self.key = key
    self.label = label
    self.votes = votes
  }
}
