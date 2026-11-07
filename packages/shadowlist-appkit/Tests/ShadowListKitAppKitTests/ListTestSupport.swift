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
 * A window whose backing scale a test sets.
 */
final class ScaleWindow: NSWindow {
  var scale: CGFloat = 2

  override var backingScaleFactor: CGFloat { scale }
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
