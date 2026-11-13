/*
 * One column's stable key, title and application-owned items.
 */
public struct ShadowListUtilsBoardColumn<Item> {
  public var key: String
  public var title: String
  public var items: [Item]

  public init(key: String, title: String, items: [Item]) {
    self.key = key
    self.title = title
    self.items = items
  }
}
