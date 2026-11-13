import UIKit

/*
 * The model of what one cell shows. Its layout is a pure function of the row and a width, safe
 * to compute off the main thread, and its view only copies the frames the layout holds.
 */
public protocol ShadowListUtilsRow {
  /*
   * The row's identity.
   */
  var key: String { get }

  /*
   * Changes when the row's content changes under the same key, like a hash of the displayed
   * fields. The list then configures that cell again.
   */
  var contentVersion: Int { get }

  /*
   * The cell class that shows the row, registered by the list the first time a row with it
   * arrives.
   */
  var viewClass: ShadowListUtilsRowView.Type { get }

  /*
   * Names the view class. Rows with the same identifier share cells.
   */
  var reuseIdentifier: String { get }

  /*
   * The row's frames for the size the list gives it across its scroll axis: the cell's width.
   * In a horizontal list width is the list's height, the layout's width is the row's size along
   * the list, and the layout's height is the height of the row's content.
   */
  func layout(width: CGFloat) -> ShadowListUtilsRowLayout
}

extension ShadowListUtilsRow {
  public var contentVersion: Int { 0 }

  public var reuseIdentifier: String { NSStringFromClass(viewClass) }
}
