import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * A cell whose height is set by its data source. sizeThatFits: answers with it, the path a list
 * without listView(_:sizeForItemAt:crossSize:) measures through.
 */
final class TestCell: ShadowListKitListCell {
  var height: CGFloat = 44
  var key = ""
  var prepared = 0
  var reconfigured = 0

  override func sizeThatFits(_ size: NSSize) -> NSSize {
    NSSize(width: size.width, height: height)
  }

  override func prepareForReuse() {
    super.prepareForReuse()
    prepared += 1
  }
}

/*
 * Items by key with a height each. Cells are measured through sizeThatFits:.
 */
class TestSource: NSObject, ShadowListKitListViewDataSource {
  var keys: [String]
  var heights: [String: CGFloat] = [:]
  var defaultHeight: CGFloat = 44
  var cellsMade = 0

  init(count: Int, prefix: String = "item") {
    keys = (0..<count).map { "\(prefix)-\($0)" }
  }

  func height(of key: String) -> CGFloat {
    heights[key] ?? defaultHeight
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { keys.count }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "cell") as! TestCell
    cell.key = keys[index]
    cell.height = height(of: keys[index])
    cellsMade += 1
    return cell
  }

  func listView(_ listView: ShadowListKitListView, reconfigureCell cell: ShadowListKitListCell, at index: Int, payload: Any?) -> Bool {
    guard let cell = cell as? TestCell else { return false }
    cell.reconfigured += 1
    return true
  }
}

/*
 * The same items with sizes from the data source.
 */
final class SizedTestSource: TestSource {
  var sizeCalls = 0

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    sizeCalls += 1
    return height(of: keys[index])
  }
}

/*
 * Records the delegate calls a test reads back.
 */
final class TestDelegate: NSObject, ShadowListKitListViewDelegate {
  var displayed: [Int] = []
  var ended: [Int] = []
  var corrections: [CGPoint] = []
  var layOuts = 0
  var scrolls = 0
  var appliesCorrections = true

  func listView(_ listView: ShadowListKitListView, willDisplay cell: ShadowListKitListCell, at index: Int) {
    displayed.append(index)
  }

  func listView(_ listView: ShadowListKitListView, didEndDisplaying cell: ShadowListKitListCell, at index: Int) {
    ended.append(index)
  }

  func listView(_ listView: ShadowListKitListView, didCorrectOffset contentOffset: CGPoint) {
    corrections.append(contentOffset)
    if appliesCorrections {
      listView.contentOffset = contentOffset
    }
  }

  func listViewDidLayOut(_ listView: ShadowListKitListView) {
    layOuts += 1
  }

  func listViewDidScroll(_ listView: ShadowListKitListView) {
    scrolls += 1
  }
}

/*
 * A window whose backing scale and key state a test sets.
 */
final class ScaleWindow: NSWindow {
  var scale: CGFloat = 2
  var key = false

  override var backingScaleFactor: CGFloat { scale }
  override var isKeyWindow: Bool { key }
}

/*
 * A cell that records the states the list gives it.
 */
final class StateCell: ShadowListKitListCell {
  var key = ""
  var highlights: [Bool] = []

  override func sizeThatFits(_ size: NSSize) -> NSSize {
    NSSize(width: size.width, height: 44)
  }

  override func setHighlighted(_ highlighted: Bool, animated: Bool) {
    super.setHighlighted(highlighted, animated: animated)
    highlights.append(highlighted)
  }
}

/*
 * Items with a type select text each, shown in StateCells.
 */
final class StateSource: NSObject, ShadowListKitListViewDataSource {
  var keys: [String]

  init(count: Int) {
    keys = (0..<count).map { "item-\($0)" }
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { keys.count }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "state") as! StateCell
    cell.key = keys[index]
    return cell
  }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 44 }
}

/*
 * Records the selection, activation and menu calls, and answers type select and menus.
 */
final class SelectionRecorder: NSObject, ShadowListKitListViewDelegate {
  var selected: [Int] = []
  var deselected: [Int] = []
  var activated: [Int] = []
  var clickedWhenActivated: [Int] = []
  var refused: Set<Int> = []
  var names: [Int: String] = [:]
  var menus = true
  var onShouldSelect: ((Int) -> Void)?

  func listView(_ listView: ShadowListKitListView, didSelectItemAt index: Int) { selected.append(index) }
  func listView(_ listView: ShadowListKitListView, didDeselectItemAt index: Int) { deselected.append(index) }

  func listView(_ listView: ShadowListKitListView, shouldSelectItemAt index: Int) -> Bool {
    onShouldSelect?(index)
    return !refused.contains(index)
  }

  func listView(_ listView: ShadowListKitListView, didActivateItemAt index: Int) {
    activated.append(index)
    clickedWhenActivated.append(listView.clickedIndex)
  }

  func listView(_ listView: ShadowListKitListView, typeSelectStringForItemAt index: Int) -> String? { names[index] }

  func listView(_ listView: ShadowListKitListView, contextMenuForItemAt index: Int) -> NSMenu? {
    guard menus, index % 2 == 0 else { return nil }
    let menu = NSMenu(title: "item-\(index)")
    menu.addItem(withTitle: "Open", action: nil, keyEquivalent: "")
    return menu
  }
}

/*
 * A list of 44 point StateCells in an offscreen window with a SelectionRecorder.
 */
@MainActor
final class SelectionHarness {
  let window: ScaleWindow
  let list: ShadowListKitListView
  let source: StateSource
  let recorder = SelectionRecorder()

  init(count: Int = 600, configure: (ShadowListKitListView) -> Void = { _ in }) {
    source = StateSource(count: count)
    let size = NSSize(width: 320, height: 480)
    window = ScaleWindow(
      contentRect: NSRect(origin: NSPoint(x: -10_000, y: -10_000), size: size), styleMask: [.borderless],
      backing: .buffered, defer: false)
    window.isReleasedWhenClosed = false
    let container = NSView(frame: NSRect(origin: .zero, size: size))
    window.contentView = container
    list = ShadowListKitListView(frame: container.bounds)
    list.register(StateCell.self, forCellReuseIdentifier: "state")
    configure(list)
    list.dataSource = source
    list.delegate = recorder
    container.addSubview(list)
    list.reloadData()
    layout()
  }

  func layout() {
    for _ in 0..<6 {
      list.needsLayout = true
      list.layoutSubtreeIfNeeded()
    }
  }

  var selection: [Int] { Array(list.selectedIndices) }

  func cell(_ index: Int) -> StateCell? { list.cellForItem(at: index) as? StateCell }

  func click(_ index: Int, _ flags: NSEvent.ModifierFlags = [], count: Int = 1) {
    list.shadowListKit_clickItem(at: index, modifierFlags: flags, clickCount: count)
  }

  /*
   * A key as the keyboard sends it. Arrows and the navigation keys carry the function flag.
   */
  func press(_ key: Key, _ flags: NSEvent.ModifierFlags = []) {
    let function: NSEvent.ModifierFlags = key.isFunction ? [.function] : []
    list.shadowListKit_pressKey(key.characters, keyCode: key.code, modifierFlags: flags.union(function))
  }

  /*
   * Whether an item's cell is shown whole inside the viewport.
   */
  func isFullyVisible(_ index: Int) -> Bool {
    guard let cell = list.cellForItem(at: index), let superview = cell.superview else { return false }
    let frame = list.convert(cell.frame, from: superview)
    return frame.minY >= -0.5 && frame.maxY <= list.bounds.height + 0.5
  }
}

/*
 * The keys the selection tests press, with their characters and key codes.
 */
struct Key {
  let characters: String
  let code: UInt16
  var isFunction: Bool { characters.unicodeScalars.first.map { $0.value >= 0xF700 } ?? false }

  static let up = Key(characters: "\u{F700}", code: 126)
  static let down = Key(characters: "\u{F701}", code: 125)
  static let left = Key(characters: "\u{F702}", code: 123)
  static let right = Key(characters: "\u{F703}", code: 124)
  static let home = Key(characters: "\u{F729}", code: 115)
  static let end = Key(characters: "\u{F72B}", code: 119)
  static let pageUp = Key(characters: "\u{F72C}", code: 116)
  static let pageDown = Key(characters: "\u{F72D}", code: 121)
  static let returnKey = Key(characters: "\r", code: 36)
  static let space = Key(characters: " ", code: 49)
  static let escape = Key(characters: "\u{1B}", code: 53)

  static func text(_ characters: String) -> Key { Key(characters: characters, code: 0) }
}

/*
 * A list in an offscreen window, laid out by hand.
 */
@MainActor
final class ListHarness {
  let window: ScaleWindow
  let list: ShadowListKitListView
  let source: TestSource
  let delegate = TestDelegate()

  init(source: TestSource, size: NSSize = NSSize(width: 320, height: 480), configure: (ShadowListKitListView) -> Void = { _ in }) {
    self.source = source
    window = ScaleWindow(
      contentRect: NSRect(origin: NSPoint(x: -10_000, y: -10_000), size: size), styleMask: [.borderless],
      backing: .buffered, defer: false)
    window.isReleasedWhenClosed = false
    let container = NSView(frame: NSRect(origin: .zero, size: size))
    window.contentView = container
    list = ShadowListKitListView(frame: container.bounds)
    list.autoresizingMask = [.width, .height]
    list.register(TestCell.self, forCellReuseIdentifier: "cell")
    configure(list)
    list.dataSource = source
    list.delegate = delegate
    container.addSubview(list)
    list.reloadData()
    layout()
  }

  /*
   * Lay out until the core settled. A settle frame waits for a display link, which an offscreen
   * window may never fire.
   */
  func layout() {
    for _ in 0..<12 {
      list.needsLayout = true
      list.layoutSubtreeIfNeeded()
    }
  }

  /*
   * Run the scroll engine's frames, a layout each, then settle.
   */
  func runScrollFrames(for duration: TimeInterval = 0.5) {
    list.shadowListKit_runScrollFrames(for: duration)
    layout()
  }

  func resize(width: CGFloat) {
    var frame = window.frame
    frame.size.width = width
    window.setFrame(frame, display: false)
    layout()
  }

  /*
   * The top of an item's cell in the list's own coordinates, or nil when it is not mounted.
   */
  func screenY(ofItem index: Int) -> CGFloat? {
    guard let cell = list.cellForItem(at: index), let superview = cell.superview else { return nil }
    return list.convert(cell.frame, from: superview).minY
  }

  func screenY(ofKey key: String) -> CGFloat? {
    guard let index = source.keys.firstIndex(of: key) else { return nil }
    return screenY(ofItem: index)
  }

  /*
   * The keys of the cells on screen and where each starts, in the list's coordinates.
   */
  func visibleKeys() -> [String: CGFloat] {
    var keys: [String: CGFloat] = [:]
    for case let cell as TestCell in list.visibleCells {
      keys[cell.key] = list.convert(cell.frame, from: cell.superview).minY
    }
    return keys
  }

  func maxShift(from before: [String: CGFloat]) -> (shift: CGFloat, kept: Int) {
    let after = visibleKeys()
    var shift: CGFloat = 0
    var kept = 0
    for (key, y) in before {
      if let moved = after[key] {
        shift = max(shift, abs(moved - y))
        kept += 1
      }
    }
    return (shift, kept)
  }
}
