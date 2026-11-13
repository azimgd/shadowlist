import CoreGraphics

/*
 * The frames of one row at one width. Subclasses add the frames and the typeset text of their
 * row. A layout never changes once made, which lets a background queue hand it to the main
 * thread.
 */
open class ShadowListUtilsRowLayout {
  public let width: CGFloat
  public let height: CGFloat

  public init(width: CGFloat, height: CGFloat) {
    self.width = width
    self.height = height
  }
}
