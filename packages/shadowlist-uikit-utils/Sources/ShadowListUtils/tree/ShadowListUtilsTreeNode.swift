import Foundation

/*
 * A file-tree node. nil children is a leaf; an empty array can still name an empty folder.
 */
public struct ShadowListUtilsTreeNode: Hashable {
  public var key: String
  public var name: String
  public var children: [ShadowListUtilsTreeNode]?
  public var kind: ShadowListUtilsTreeNodeKind?

  public init(key: String, name: String, children: [ShadowListUtilsTreeNode]? = nil, kind: ShadowListUtilsTreeNodeKind? = nil) {
    self.key = key
    self.name = name
    self.children = children
    self.kind = kind
  }
}
