import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

/*
 * The width every layout test lays a row out at.
 */
let testWidth: CGFloat = 390

/*
 * Puts a list into a window of a phone's size and lays it out, the way a screen would.
 */
@MainActor
@discardableResult
func host(_ list: ShadowListKitListView, size: CGSize = CGSize(width: testWidth, height: 800)) -> UIWindow {
  let window = UIWindow(frame: CGRect(origin: .zero, size: size))
  list.frame = window.bounds
  window.addSubview(list)
  window.isHidden = false
  list.layoutIfNeeded()
  return window
}

@MainActor
extension ShadowListUtilsList {
  /*
   * The keys the list shows, in order.
   */
  var keys: [String] { rows.map(\.key) }

  /*
   * The cell of an item as a row view, or nil when the item is not mounted.
   */
  func rowView(at index: Int) -> ShadowListUtilsRowView? {
    cellForItem(at: index) as? ShadowListUtilsRowView
  }

  /*
   * What the kit asks and tells the list's own data source and delegate, for a test that
   * stands in for a gesture.
   */
  func tap(_ index: Int) {
    delegate?.listView?(self, didSelectItemAt: index)
  }

  func drop(from sourceIndex: Int, to destinationIndex: Int) {
    delegate?.listView?(self, moveItemAt: sourceIndex, to: destinationIndex)
  }

  func leadingActions(at index: Int) -> [ShadowListKitSwipeAction] {
    delegate?.listView?(self, leadingSwipeActionsForItemAt: index)?.actions ?? []
  }

  func trailingActions(at index: Int) -> [ShadowListKitSwipeAction] {
    delegate?.listView?(self, trailingSwipeActionsForItemAt: index)?.actions ?? []
  }

  func menu(at index: Int) -> UIMenu? {
    delegate?.listView?(self, contextMenuForItemAt: index) ?? nil
  }
}

extension ShadowListKitSwipeAction {
  /*
   * Run the action the way a press on its button does.
   */
  @MainActor
  func perform() {
    handler(self) { _ in }
  }
}

/*
 * A text layout is as tall as its lines: line breaking differs per platform, the line height
 * does not.
 */
func assertLines(
  _ layout: ShadowListKitTextLayout?, _ style: ShadowListUtilsTextStyle, maxLines: Int = .max, file: StaticString = #filePath,
  line: UInt = #line
) {
  guard let layout else {
    return XCTFail("no text layout", file: file, line: line)
  }
  XCTAssertTrue((1...maxLines).contains(Int(layout.lineCount)), "\(layout.lineCount) lines, at most \(maxLines) expected", file: file, line: line)
  XCTAssertEqual(layout.size.height, CGFloat(layout.lineCount) * style.scaledLineHeight(), accuracy: 0.01, file: file, line: line)
}

/*
 * Runs a body with the library's text size override set, then clears it.
 */
func withTextSize<Result>(_ scale: CGFloat, _ body: () throws -> Result) rethrows -> Result {
  ShadowListUtilsPlatform.textScaleOverride = scale
  defer { ShadowListUtilsPlatform.textScaleOverride = nil }
  return try body()
}

/*
 * Runs a body with the library's layout direction override set, then clears it.
 */
func withRightToLeft<Result>(_ body: () throws -> Result) rethrows -> Result {
  ShadowListUtilsPlatform.isRightToLeftOverride = true
  defer { ShadowListUtilsPlatform.isRightToLeftOverride = nil }
  return try body()
}

/*
 * A row that counts how often its view was filled, by key.
 */
struct CountingRow: ShadowListUtilsRow {
  let key: String
  var contentVersion = 0
  var height: CGFloat = 44

  var viewClass: ShadowListUtilsRowView.Type { CountingRowView.self }

  func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    ShadowListUtilsRowLayout(width: width, height: height)
  }
}

final class CountingRowView: ShadowListUtilsRowView {
  static var fills: [String: Int] = [:]

  override func fill(_ row: ShadowListUtilsRow) {
    Self.fills[row.key, default: 0] += 1
  }
}

/*
 * The folder of vector files the JS, C++ and native tests share.
 */
enum Vectors {
  static let folder = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
    .appendingPathComponent("shadowlist-core-tests/vectors")

  /*
   * The parsed file, or a skip when it has not been written yet.
   */
  static func read(_ name: String) throws -> Any {
    let url = folder.appendingPathComponent(name)
    guard FileManager.default.fileExists(atPath: url.path) else {
      throw XCTSkip("\(name) is not in \(folder.path)")
    }
    return try JSONSerialization.jsonObject(with: Data(contentsOf: url))
  }

  static func cases(_ name: String) throws -> [[String: Any]] {
    let parsed = try read(name)
    return try XCTUnwrap(parsed as? [[String: Any]], "\(name) is a list of cases")
  }
}

/*
 * Fixtures the template tests share.
 */
enum Fixture {
  static func contact(_ index: Int) -> ShadowListUtilsContactItem {
    ShadowListUtilsContactItem(key: "contact-\(index)", name: "Person \(index)", subtitle: "Seat \(index)")
  }

  static func contacts(_ count: Int) -> [ShadowListUtilsContactItem] {
    (0..<count).map(contact)
  }
}
