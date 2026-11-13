import UIKit

/*
 * A tile of a reorder grid: a colored swatch with an optional label on it and a title under it.
 * Without a color the swatch takes the palette color of the label, or of the title. aspectRatio
 * is the swatch's width over its height, 1 when nil.
 */
public struct ShadowListUtilsReorderTileItem: Hashable {
  public var key: String
  public var title: String
  public var label: String?
  public var color: UIColor?
  public var aspectRatio: CGFloat?

  public init(key: String, title: String, label: String? = nil, color: UIColor? = nil, aspectRatio: CGFloat? = nil) {
    self.key = key
    self.title = title
    self.label = label
    self.color = color
    self.aspectRatio = aspectRatio
  }
}
