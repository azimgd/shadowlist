import UIKit

/*
 * A visible node's metadata and its expansion action, the same values a TreeList row renders.
 */
public struct ShadowListUtilsTreeListRenderItemInfo {
  public let item: ShadowListUtilsTreeNode
  public let depth: Int
  public let indent: CGFloat
  public let isExpanded: Bool
  public let hasChildren: Bool
  public let toggle: () -> Void
}
