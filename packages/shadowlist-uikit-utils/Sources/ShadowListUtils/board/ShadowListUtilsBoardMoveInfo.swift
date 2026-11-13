/*
 * A request to move item before destinationKey, or to the end for an empty key.
 * destinationIndex is the fallback when that key no longer exists.
 */
public struct ShadowListUtilsBoardMoveInfo<Item> {
  public let item: Item
  public let sourceColumnKey: String
  public let destinationColumnKey: String
  public let destinationKey: String
  public let destinationIndex: Int

  public init(item: Item, sourceColumnKey: String, destinationColumnKey: String, destinationKey: String, destinationIndex: Int) {
    self.item = item
    self.sourceColumnKey = sourceColumnKey
    self.destinationColumnKey = destinationColumnKey
    self.destinationKey = destinationKey
    self.destinationIndex = destinationIndex
  }
}
