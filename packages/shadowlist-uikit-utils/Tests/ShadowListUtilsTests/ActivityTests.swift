import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class ActivityTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light
  private func item(_ key: String = "activity", read: Bool = false) -> ShadowListUtilsActivityItem {
    ShadowListUtilsActivityItem(key: key, actor: ShadowListUtilsActivityActor(name: "Ada Lovelace"),
      action: "commented on your itinerary", text: "A long activity preview that wraps over multiple lines on a phone.",
      createdAtMs: 0, read: read)
  }

  func testTheActionUsesRegularMetricsWhileTheActorKeepsItsEmphasis() throws {
    let item = ShadowListUtilsActivityItem(key: "one", actor: ShadowListUtilsActivityActor(name: "Mara"),
      action: "followed a remarkably wonderful itinerary", createdAtMs: 0)
    let row = ShadowListUtilsActivityRow(item: item, theme: theme, formatTime: { _ in "" })
    let layout = try XCTUnwrap(row.layout(width: 2000) as? ShadowListUtilsActivityRowLayout)
    let text = "\(item.actor.name) \(item.action)"
    let regular = theme.typography.subhead.layout(text, width: 2000)
    let semibold = theme.typography.subhead.with(.semibold).layout(text, width: 2000)
    XCTAssertGreaterThan(layout.title.size.width, regular.size.width)
    XCTAssertLessThan(layout.title.size.width, semibold.size.width)
    let narrow = try XCTUnwrap(withTextSize(1.6) { row.layout(width: 230) } as? ShadowListUtilsActivityRowLayout)
    XCTAssertGreaterThan(narrow.title.lineCount, 1)
  }

  func testTheListShowsItsItemsAndSelectsOnlyWhenAsked() {
    let list = ShadowListUtilsActivityList(theme: theme)
    list.items = [item("one"), item("two")]
    host(list)
    XCTAssertEqual(list.keys, ["one", "two"])
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsActivityRowView)
    XCTAssertFalse(list.allowsSelection)
    var pressed: [String] = []
    list.onPressItem = { pressed.append($0.key) }
    list.tap(0)
    XCTAssertEqual(pressed, ["one"])
    XCTAssertFalse(list.items[0].read)
    list.onPressItem = nil
    XCTAssertFalse(list.allowsSelection)
  }

  func testReadAndLabelChangesReplaceTheVisibleState() throws {
    let list = ShadowListUtilsActivityList(theme: theme)
    list.formatTime = { _ in "5m" }
    list.items = [item()]
    host(list)
    XCTAssertTrue(try XCTUnwrap(list.rowView(at: 0)?.accessibilityLabel).contains("Unread"))
    XCTAssertTrue(try XCTUnwrap(list.rowView(at: 0)?.accessibilityLabel).contains("5m"))
    list.items = [item(read: true)]
    list.layoutIfNeeded()
    XCTAssertFalse(try XCTUnwrap(list.rowView(at: 0)?.accessibilityLabel).contains("Unread"))
    list.items = [item()]
    list.labels.unread = "New activity"
    list.layoutIfNeeded()
    XCTAssertTrue(try XCTUnwrap(list.rowView(at: 0)?.accessibilityLabel).contains("New activity"))
    XCTAssertFalse(try XCTUnwrap(list.rowView(at: 0)).accessibilityTraits.contains(.button))
    list.onPressItem = { _ in }
    list.layoutIfNeeded()
    XCTAssertTrue(try XCTUnwrap(list.rowView(at: 0)).accessibilityTraits.contains(.button))
  }

  func testAnAbsentPreviewHasNoTextAndLargeTextGrowsTheRow() throws {
    var activity = item()
    let row = ShadowListUtilsActivityRow(item: activity, theme: theme, formatTime: { _ in "now" })
    let regular = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsActivityRowLayout)
    let large = try XCTUnwrap(withTextSize(1.6) { row.layout(width: testWidth) } as? ShadowListUtilsActivityRowLayout)
    XCTAssertGreaterThan(large.height, regular.height)
    XCTAssertGreaterThan(large.title.lineCount, regular.title.lineCount)
    activity.text = nil
    let empty = try XCTUnwrap(ShadowListUtilsActivityRow(item: activity, theme: theme).layout(width: testWidth) as? ShadowListUtilsActivityRowLayout)
    XCTAssertNil(empty.text)
    XCTAssertEqual(empty.textFrame, .zero)
  }

  func testARightToLeftRowMirrorsItsFrames() throws {
    let row = ShadowListUtilsActivityRow(item: item(), theme: theme, formatTime: { _ in "now" })
    let regular = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsActivityRowLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { row.layout(width: testWidth) } as? ShadowListUtilsActivityRowLayout)
    for (first, second) in [(regular.avatarFrame, mirrored.avatarFrame), (regular.titleFrame, mirrored.titleFrame),
                            (regular.timeFrame, mirrored.timeFrame), (regular.unreadDotFrame, mirrored.unreadDotFrame)] {
      XCTAssertEqual(first.minX, testWidth - second.maxX, accuracy: 0.001)
      XCTAssertEqual(first.height, second.height)
    }
  }
}
