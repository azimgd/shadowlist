import ShadowListKit
import UIKit

/*
 * A tree node at the depth and expansion state the shared tree source returned. icon replaces
 * the folder or file glyph. The host keeps labels and direction in its own locale.
 */
public struct ShadowListUtilsTreeRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsTreeNode
  public let depth: Int
  public let hasChildren: Bool
  public let isExpanded: Bool
  public let indentWidth: CGFloat
  public let interactive: Bool
  public let icon: UIImage?
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsTreeLabels

  public init(item: ShadowListUtilsTreeNode, depth: Int, hasChildren: Bool, isExpanded: Bool, indentWidth: CGFloat = 16,
              interactive: Bool = false, icon: UIImage? = nil, theme: ShadowListUtilsTheme = .system,
              labels: ShadowListUtilsTreeLabels = ShadowListUtilsTreeLabels()) {
    self.item = item
    self.depth = depth
    self.hasChildren = hasChildren
    self.isExpanded = isExpanded
    self.indentWidth = indentWidth
    self.interactive = interactive
    self.icon = icon
    self.theme = theme
    self.labels = labels
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsTreeRowView.self }
  public var contentVersion: Int {
    var hasher = Hasher()
    hasher.combine(item.key)
    hasher.combine(item.name)
    hasher.combine(item.kind)
    hasher.combine(item.children?.count)
    hasher.combine(depth)
    hasher.combine(hasChildren)
    hasher.combine(isExpanded)
    hasher.combine(indentWidth)
    hasher.combine(interactive)
    hasher.combine(icon)
    hasher.combine(labels.expand)
    hasher.combine(labels.collapse)
    return hasher.finalize()
  }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let direction = ShadowListUtilsDirection(width: width)
    let start = max(0, indentWidth) * CGFloat(depth)
    let end = theme.spacing.lg
    let count = item.children.map { theme.typography.footnote.layout(String($0.count), width: max(1, width / 4), maxLines: 1) }
    let countWidth = count?.size.width ?? 0
    let textX = start + 22 + 24 + theme.spacing.sm
    let textWidth = max(1, width - textX - end - countWidth - (count == nil ? 0 : theme.spacing.sm))
    let name = theme.typography.callout.layout(item.name, width: textWidth, maxLines: ShadowListUtilsPlatform.isLargeText ? 0 : 1)
    let height = max(44, name.size.height + theme.spacing.xs * 2)
    return ShadowListUtilsTreeRowLayout(
      width: width, height: height, name: name,
      nameFrame: direction.place(CGRect(x: textX, y: (height - name.size.height) / 2, width: name.size.width, height: name.size.height)),
      chevronFrame: direction.place(CGRect(x: start + 4, y: (height - 14) / 2, width: 14, height: 14)),
      iconFrame: direction.place(CGRect(x: start + 22, y: (height - 20) / 2, width: 20, height: 20)), count: count,
      countFrame: direction.place(CGRect(x: width - end - countWidth, y: (height - (count?.size.height ?? 0)) / 2,
                                       width: countWidth, height: count?.size.height ?? 0)))
  }
}
