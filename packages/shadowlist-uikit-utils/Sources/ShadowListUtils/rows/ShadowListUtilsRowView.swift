import ShadowListKit
import UIKit

/*
 * The cell that shows a row. Subclasses set the content in fill and the frames in apply, and
 * never measure anything themselves.
 */
open class ShadowListUtilsRowView: ShadowListKitListCell {
  public private(set) var row: ShadowListUtilsRow?

  /*
   * The layout shown and the cross size it was made for: the width, or the height in a
   * horizontal list.
   */
  private var rowLayout: ShadowListUtilsRowLayout?
  private var layoutCrossSize: CGFloat = -1

  /*
   * The background at rest and while the row is pressed. Without a pressed color the row does
   * not highlight.
   */
  var restingBackgroundColor: ShadowListUtilsColor? {
    didSet { updateBackground() }
  }
  var pressedBackgroundColor: ShadowListUtilsColor? {
    didSet { updateBackground() }
  }

  /*
   * Whether the pressed background shows. A subclass adds its own states, like a selected row
   * of a list that is editing.
   */
  var showsPressedBackground: Bool { isHighlighted }

  public required override init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
  }

  /*
   * Show a row. With a layout the frames are taken as they are. Without one the layout is
   * computed on the main thread the first time the view knows its size, like a self-sizing
   * cell.
   */
  public func configure(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout?) {
    configure(row, layout: layout, crossSize: layout?.width ?? -1)
  }

  /*
   * The same from a list, which knows the cross size the layout was made for.
   */
  func configure(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout?, crossSize: CGFloat) {
    self.row = row
    rowLayout = layout
    layoutCrossSize = crossSize
    fill(row)
    setNeedsLayout()
  }

  /*
   * Take a row that shows the same content under the same key, like one built again with a new
   * closure. Nothing is filled or laid out.
   */
  func replaceRow(_ row: ShadowListUtilsRow) {
    self.row = row
  }

  /*
   * Set the content, like colors and images, without frames.
   */
  open func fill(_ row: ShadowListUtilsRow) {}

  /*
   * Set the frames from the layout.
   */
  open func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {}

  open override func prepareForReuse() {
    super.prepareForReuse()
    row = nil
    rowLayout = nil
    layoutCrossSize = -1
  }

  /*
   * The layout's size along the scroll axis. A horizontal list asks with an unbounded width.
   */
  open override func sizeThatFits(_ size: CGSize) -> CGSize {
    let horizontal = size.width >= CGFloat.greatestFiniteMagnitude / 2
    guard let layout = ensureLayout(crossSize: horizontal ? size.height : size.width) else {
      return horizontal ? CGSize(width: 0, height: size.height) : CGSize(width: size.width, height: 0)
    }
    return horizontal ? CGSize(width: layout.width, height: size.height) : CGSize(width: size.width, height: layout.height)
  }

  open override func layoutSubviews() {
    super.layoutSubviews()
    guard let row else { return }
    let horizontal = enclosingList?.isHorizontal ?? false
    if let layout = ensureLayout(crossSize: horizontal ? bounds.height : bounds.width) {
      apply(row, layout: layout)
    }
  }

  /*
   * The layout for a cross size: the one the view holds when it was made for it, otherwise a
   * new one.
   */
  private func ensureLayout(crossSize: CGFloat) -> ShadowListUtilsRowLayout? {
    guard let row, crossSize > 0 else { return nil }
    if let rowLayout, layoutCrossSize == crossSize {
      return rowLayout
    }
    let layout = row.layout(width: crossSize)
    rowLayout = layout
    layoutCrossSize = crossSize
    return layout
  }

  /*
   * The list the cell is in, or nil while it waits in the reuse pool.
   */
  var enclosingList: ShadowListKitListView? {
    var view = superview
    while let current = view {
      if let list = current as? ShadowListKitListView {
        return list
      }
      view = current.superview
    }
    return nil
  }

  /*
   * The row left the screen. A row with state of its own, like a nested list's position, saves
   * it.
   */
  func didEndDisplaying(in list: ShadowListUtilsList) {}

  // MARK: - Highlight

  open override func setHighlighted(_ highlighted: Bool, animated: Bool) {
    super.setHighlighted(highlighted, animated: animated)
    updateBackground()
  }

  /*
   * The pressed color shows the moment a press starts and leaves the moment it ends.
   */
  func updateBackground() {
    backgroundColor = showsPressedBackground ? pressedBackgroundColor ?? restingBackgroundColor : restingBackgroundColor
  }

  // MARK: - Accessibility

  /*
   * A row of an item in a list that selects on a tap reads as a button.
   */
  open override var accessibilityTraits: UIAccessibilityTraits {
    get {
      let pressable = index != NSNotFound && enclosingList?.allowsSelection == true
      return pressable ? super.accessibilityTraits.union(.button) : super.accessibilityTraits
    }
    set { super.accessibilityTraits = newValue }
  }

  /*
   * The actions the row's list offers a screen reader for its item.
   */
  open override var accessibilityCustomActions: [UIAccessibilityCustomAction]? {
    get {
      let own = super.accessibilityCustomActions
      guard index != NSNotFound, let list = enclosingList as? ShadowListUtilsList else { return own }
      let actions = list.accessibilityActions(at: index)
      return actions.isEmpty ? own : (own ?? []) + actions
    }
    set { super.accessibilityCustomActions = newValue }
  }
}
