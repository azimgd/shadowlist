import ShadowListKit
import UIKit

/*
 * A chat thread: an inverted list that opens at its end and keeps what is on screen in place
 * while messages arrive. Items are oldest first.
 */
public final class ShadowListUtilsChatList: ShadowListUtilsList {
  /*
   * The messages. Setting them shows them without an animation.
   */
  public var items: [ShadowListUtilsChatMessage] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }

  /*
   * Show these items. animated animates the change from the items before.
   */
  public func setItems(_ items: [ShadowListUtilsChatMessage], animated: Bool) {
    let rows = rows(for: items, previousItems: storedItems, previousRows: rows, key: \.key, make: makeRow)
    storedItems = items
    setRows(rows, animated: animated)
  }

  /*
   * Runs for a tapped message. Without it the messages do not select.
   */
  public var onPressItem: ((ShadowListUtilsChatMessage) -> Void)? {
    didSet { allowsSelection = onPressItem != nil }
  }

  /*
   * Replaces the bubble with a row of the app's own for every item.
   */
  public var rowForItem: ((ShadowListUtilsChatMessage) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsChatLabels() {
    didSet { reloadRows() }
  }

  /*
   * Runs for a tap on the line under a failed message.
   */
  public var onRetryMessage: ((ShadowListUtilsChatMessage) -> Void)?

  /*
   * Runs for a hold on a message's bubble. The lift that ends the hold is not a tap.
   */
  public var onLongPressMessage: ((ShadowListUtilsChatMessage) -> Void)?

  /*
   * Gives a message the small line inside its bubble, like its time, or nil for none.
   */
  public var captionForItem: ((ShadowListUtilsChatMessage) -> String?)? {
    didSet { reloadRows() }
  }

  /*
   * Whether messages from others show their sender's avatar and name.
   */
  public var showsAuthor = true {
    didSet {
      if showsAuthor != oldValue {
        rebuildRows()
      }
    }
  }

  private var storedItems: [ShadowListUtilsChatMessage] = []

  /*
   * Where the last touch came down, in the list's coordinates.
   */
  private var touchPoint: CGPoint?

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    isInverted = true
    allowsSelection = false
  }

  override func makeAdapter() -> ShadowListUtilsListAdapter {
    ShadowListUtilsLongPressListAdapter()
  }

  private func makeRow(_ item: ShadowListUtilsChatMessage) -> ShadowListUtilsRow {
    rowForItem?(item)
      ?? ShadowListUtilsChatBubble(item: item, caption: captionForItem?(item), showsAuthor: showsAuthor, theme: theme, labels: labels)
  }

  override func makeRows() -> [ShadowListUtilsRow]? {
    storedItems.map(makeRow)
  }

  override func didSelectItem(at index: Int) {
    if storedItems.indices.contains(index) {
      onPressItem?(storedItems[index])
    }
    deselectItem(at: index, animated: true)
  }

  public override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
    touchPoint = point
    return super.hitTest(point, with: event)
  }

  /*
   * The kit tells of a hold on an item, and its lift selects nothing. A hold counts when it is
   * on the message's bubble. It goes to onLongPressMessage with the system's tick.
   */
  override func didLongPressItem(at index: Int) {
    guard let onLongPressMessage, storedItems.indices.contains(index) else { return }
    if let touchPoint, let view = cellForItem(at: index) as? ShadowListUtilsChatBubbleView,
      !view.bubbleContains(convert(touchPoint, to: view))
    {
      return
    }
    UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    onLongPressMessage(storedItems[index])
  }

  /*
   * A screen reader retries a failed message through an action.
   */
  override func accessibilityActions(at index: Int) -> [UIAccessibilityCustomAction] {
    guard let onRetryMessage, storedItems.indices.contains(index), storedItems[index].status == .failed else { return [] }
    let item = storedItems[index]
    return [UIAccessibilityCustomAction(name: labels.retryHint) { _ in
      onRetryMessage(item)
      return true
    }]
  }
}
