/*
 * dragHint is what a screen reader says a row is for. The others name the actions that move an
 * item by one place: up and down in a list, earlier and later in a grid.
 */
public struct ShadowListUtilsReorderLabels: Hashable {
  public var dragHint: String = "Press and hold, then drag to reorder"
  public var moveUp: String = "Move up"
  public var moveDown: String = "Move down"
  public var moveEarlier: String = "Move earlier"
  public var moveLater: String = "Move later"

  public init() {}
}
