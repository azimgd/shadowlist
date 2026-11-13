/*
 * The one array move the library uses.
 */
enum ShadowListUtilsArrayMove {
  /*
   * The items with the one at sourceIndex taken out and put back at destinationIndex. An index
   * outside the array returns the items as they are.
   */
  static func moved<T>(_ items: [T], from sourceIndex: Int, to destinationIndex: Int) -> [T] {
    guard items.indices.contains(sourceIndex), items.indices.contains(destinationIndex), sourceIndex != destinationIndex else {
      return items
    }
    var result = items
    result.insert(result.remove(at: sourceIndex), at: destinationIndex)
    return result
  }
}
