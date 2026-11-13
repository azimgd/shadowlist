import Foundation
import ShadowListKit

/*
 * Apply the shared board move plan to application objects. The core resolves keys and indices;
 * the host only removes matching objects and inserts the supplied item at the resolved place.
 */
public func shadowListUtilsMoveBoardItem<Item>(
  _ columns: [ShadowListUtilsBoardColumn<Item>], move: ShadowListUtilsBoardMoveInfo<Item>, keyExtractor: (Item) -> String
) -> [ShadowListUtilsBoardColumn<Item>] {
  guard let destination = columns.firstIndex(where: { $0.key == move.destinationColumnKey }) else { return columns }
  let key = keyExtractor(move.item)
  let plan = ShadowListKitBoardMovePlan.plan(columns: columns.map { $0.items.map(keyExtractor) }, itemKey: key,
                                           destinationColumn: UInt(destination), destinationKey: move.destinationKey,
                                           destinationIndex: move.destinationIndex < 0 ? UInt(NSNotFound) : UInt(move.destinationIndex))
  guard !plan.isEmpty else { return columns }
  var next = columns
  for index in next.indices { next[index].items.removeAll { keyExtractor($0) == key } }
  next[Int(plan.destinationColumn)].items.insert(move.item, at: Int(plan.destinationIndex))
  return next
}
