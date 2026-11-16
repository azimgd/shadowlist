import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class InboxTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  private func message(_ index: Int, read: Bool = true, flagged: Bool = false) -> ShadowListUtilsInboxMessage {
    ShadowListUtilsInboxMessage(
      key: "mail-\(index)", sender: "Sender \(index)", subject: "Subject \(index)",
      preview: "A preview that is long enough to wrap onto a second line of the row for certain.",
      receivedAtMs: Date().timeIntervalSince1970 * 1000 - 5 * 60_000, read: read, flagged: flagged)
  }

  private func row(_ item: ShadowListUtilsInboxMessage, editing: Bool = false) -> ShadowListUtilsInboxRow {
    ShadowListUtilsInboxRow(item: item, editing: editing, theme: theme, labels: ShadowListUtilsInboxLabels())
  }

  func testTheListShowsItsItems() {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = (0..<5).map { message($0) }
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<5).map { "mail-\($0)" })
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsInboxRowView)
  }

  func testTheListShowsSeparatorsAndSelectsOnlyWhenAsked() {
    let list = ShadowListUtilsInboxList(theme: theme)
    XCTAssertTrue(list.showsSeparators)
    XCTAssertEqual(list.separatorInsetStart, 68)
    XCTAssertFalse(list.allowsSelection)
    XCTAssertFalse(list.allowsMultipleSelection)
    list.onPressItem = { _ in }
    XCTAssertTrue(list.allowsSelection)
    list.onPressItem = nil
    list.isEditing = true
    XCTAssertTrue(list.allowsSelection)
    XCTAssertTrue(list.allowsMultipleSelection)
    list.isEditing = false
    XCTAssertFalse(list.allowsSelection)
    XCTAssertFalse(list.allowsMultipleSelection)
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    var items = (0..<4).map { message($0, read: false) }
    list.items = items
    list.layoutIfNeeded()
    let untouched = list.rowView(at: 0)
    items[1].read = true
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 1)?.row as? ShadowListUtilsInboxRow)?.item.read, true)
    XCTAssertTrue(list.rowView(at: 0) === untouched)
    items.remove(at: 0)
    list.setItems(items, animated: true)
    XCTAssertEqual(list.keys, ["mail-1", "mail-2", "mail-3"])
  }

  func testATapPressesAnItemWhileTheListIsNotEditing() {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = (0..<3).map { message($0) }
    var pressed: [String] = []
    var changes = 0
    list.onPressItem = { pressed.append($0.key) }
    list.onSelectionChange = { _ in changes += 1 }
    list.tap(2)
    XCTAssertEqual(pressed, ["mail-2"])
    XCTAssertEqual(changes, 0)
  }

  func testATapSelectsWhileEditingAndReportsTheSelectedKeys() {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = (0..<4).map { message($0) }
    list.layoutIfNeeded()
    var pressed = 0
    var changes: [Set<String>] = []
    list.onPressItem = { _ in pressed += 1 }
    list.onSelectionChange = { changes.append($0) }
    list.setEditing(true, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 0)?.row as? ShadowListUtilsInboxRow)?.editing, true)

    list.selectItem(at: 1, animated: false)
    list.tap(1)
    list.selectItem(at: 3, animated: false)
    list.tap(3)
    XCTAssertEqual(changes, [["mail-1"], ["mail-1", "mail-3"]])
    XCTAssertEqual(list.selectedKeys, ["mail-1", "mail-3"])
    list.deselectItem(at: 1, animated: false)
    list.delegate?.listView?(list, didDeselectItemAt: 1)
    XCTAssertEqual(changes.last, ["mail-3"])
    XCTAssertEqual(pressed, 0)
  }

  func testSettingSelectedKeysSelectsWithoutACallback() {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = (0..<4).map { message($0) }
    var changes = 0
    list.onSelectionChange = { _ in changes += 1 }
    list.setEditing(true, animated: false)
    list.selectedKeys = ["mail-0", "mail-2"]
    XCTAssertEqual(Array(list.selectedIndices), [0, 2])
    list.selectedKeys = ["mail-3"]
    XCTAssertEqual(Array(list.selectedIndices), [3])
    XCTAssertEqual(changes, 0)
  }

  func testLeavingEditingClearsTheSelection() {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = (0..<4).map { message($0) }
    list.layoutIfNeeded()
    var changes: [Set<String>] = []
    list.onSelectionChange = { changes.append($0) }
    list.setEditing(true, animated: false)
    list.selectedKeys = ["mail-1"]
    list.setEditing(false, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual(list.selectedIndices.count, 0)
    XCTAssertEqual(list.selectedKeys, [])
    XCTAssertEqual(changes, [[]])
    XCTAssertEqual(list.rowView(at: 1)?.isSelected, false)
    // Nothing was selected the second time, and nothing is reported.
    list.setEditing(true, animated: false)
    list.setEditing(false, animated: false)
    XCTAssertEqual(changes, [[]])
  }

  func testTheSwipeActionsFollowTheItemsStateAndTheClosuresThatAreSet() {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = [message(0, read: true), message(1, read: false, flagged: true)]
    XCTAssertTrue(list.leadingActions(at: 0).isEmpty)
    XCTAssertTrue(list.trailingActions(at: 0).isEmpty)
    var calls: [String] = []
    list.onToggleRead = { calls.append("read \($0.key)") }
    list.onToggleFlag = { calls.append("flag \($0.key)") }
    list.onDelete = { calls.append("delete \($0.key)") }
    XCTAssertEqual(list.leadingActions(at: 0).map(\.title), ["Unread"])
    XCTAssertEqual(list.leadingActions(at: 1).map(\.title), ["Read"])
    XCTAssertEqual(list.leadingActions(at: 0).first?.backgroundColor, theme.colors.blue)
    let trailing = list.trailingActions(at: 1)
    XCTAssertEqual(trailing.map(\.title), ["Delete", "Unflag"])
    XCTAssertEqual(trailing.first?.style, .destructive)
    XCTAssertEqual(trailing.first?.backgroundColor, theme.colors.red)
    XCTAssertEqual(trailing.last?.backgroundColor, theme.colors.orange)
    XCTAssertEqual(list.trailingActions(at: 0).map(\.title), ["Delete", "Flag"])
    list.leadingActions(at: 0).first?.perform()
    trailing.first?.perform()
    trailing.last?.perform()
    XCTAssertEqual(calls, ["read mail-0", "delete mail-1", "flag mail-1"])
    list.onDelete = nil
    XCTAssertEqual(list.trailingActions(at: 0).map(\.title), ["Flag"])
    list.setEditing(true, animated: false)
    XCTAssertTrue(list.leadingActions(at: 0).isEmpty)
    XCTAssertTrue(list.trailingActions(at: 0).isEmpty)
  }

  func testTheMenuListsTheActionsInOrderUnderTheSubject() throws {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = [message(0, read: true), message(1, read: false, flagged: true)]
    XCTAssertNil(list.menu(at: 0))
    var selected: [Set<String>] = []
    list.onReply = { _ in }
    list.onToggleRead = { _ in }
    list.onToggleFlag = { _ in }
    list.onDelete = { _ in }
    list.onSelectionChange = { selected.append($0) }
    let menu = try XCTUnwrap(list.menu(at: 0))
    XCTAssertEqual(menu.title, "Subject 0")
    XCTAssertEqual(menu.children.map(\.title), ["Reply", "Mark as Unread", "Flag", "Select", "Delete"])
    XCTAssertEqual((menu.children.last as? UIAction)?.attributes, .destructive)
    XCTAssertEqual(try XCTUnwrap(list.menu(at: 1)).children.map(\.title), ["Reply", "Mark as Read", "Unflag", "Select", "Delete"])
    list.onReply = nil
    list.onSelectionChange = nil
    XCTAssertEqual(try XCTUnwrap(list.menu(at: 0)).children.map(\.title), ["Mark as Unread", "Flag", "Delete"])
    list.setEditing(true, animated: false)
    XCTAssertNil(list.menu(at: 0))
  }

  func testTheMenusSelectEntryReportsItsOneKey() throws {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    list.items = (0..<3).map { message($0) }
    list.layoutIfNeeded()
    var selected: [Set<String>] = []
    list.onSelectionChange = { selected.append($0) }
    let actions = try XCTUnwrap(list.rowView(at: 2)?.accessibilityCustomActions)
    XCTAssertEqual(actions.map(\.name), ["Select"])
    XCTAssertEqual(actions[0].actionHandler?(actions[0]), true)
    XCTAssertEqual(selected, [["mail-2"]])
    XCTAssertFalse(list.isEditing)
  }

  func testAnActionActsOnTheItemAsItIsWhenItRuns() throws {
    let list = ShadowListUtilsInboxList(theme: theme)
    host(list)
    var items = [message(0, read: false), message(1)]
    list.items = items
    list.layoutIfNeeded()
    var toggled: [ShadowListUtilsInboxMessage] = []
    var deleted: [String] = []
    list.onToggleRead = { toggled.append($0) }
    list.onDelete = { deleted.append($0.key) }
    let swipe = try XCTUnwrap(list.leadingActions(at: 0).first)
    let menu = try XCTUnwrap(list.menu(at: 0))
    let delete = try XCTUnwrap(list.trailingActions(at: 1).first)
    // The message changes and another one leaves while the actions are open.
    items[0].subject = "Edited"
    items.remove(at: 1)
    list.items = items
    swipe.perform()
    XCTAssertEqual(toggled.map(\.subject), ["Edited"])
    let entry = try XCTUnwrap(menu.children.first as? UIAction)
    entry.performWithSender(nil, target: nil)
    XCTAssertEqual(toggled.map(\.subject), ["Edited", "Edited"])
    delete.perform()
    XCTAssertEqual(deleted, [])
  }

  func testTheRowLayoutHoldsItsMetrics() throws {
    let typography = theme.typography
    let layout = try XCTUnwrap(row(message(0, read: false, flagged: true)).layout(width: testWidth) as? ShadowListUtilsInboxRowLayout)
    XCTAssertEqual(layout.unreadDotFrame, CGRect(x: 9, y: 12 + 16, width: 10, height: 10))
    XCTAssertEqual(layout.checkFrame, CGRect(x: 3, y: 12 + 9, width: 22, height: 22))
    XCTAssertEqual(layout.avatarFrame, CGRect(x: 28, y: 12, width: 40, height: 40))
    XCTAssertEqual(layout.senderFrame.origin, CGPoint(x: 80, y: 12))
    XCTAssertEqual(layout.timeFrame.maxX, testWidth - 16)
    XCTAssertEqual(layout.flagFrame.size, CGSize(width: 8, height: 8))
    XCTAssertEqual(layout.flagFrame.maxX, layout.timeFrame.minX - 4)
    XCTAssertEqual(layout.flagFrame.midY, 12 + typography.body.lineHeight / 2)
    XCTAssertEqual(layout.subjectFrame.origin, CGPoint(x: 80, y: 12 + typography.body.lineHeight))
    XCTAssertEqual(layout.previewFrame.origin, CGPoint(x: 80, y: 12 + typography.body.lineHeight + typography.subhead.lineHeight))
    XCTAssertEqual(layout.preview.lineCount, 2)
    XCTAssertEqual(layout.height, 12 + typography.body.lineHeight + typography.subhead.lineHeight * 3 + 12)
    XCTAssertEqual(layout.sender.lineCount, 1)
    XCTAssertEqual(layout.subject.lineCount, 1)
  }

  func testARightToLeftRowMirrorsItsFrames() throws {
    let mail = row(message(0, read: false, flagged: true))
    let plain = try XCTUnwrap(mail.layout(width: testWidth) as? ShadowListUtilsInboxRowLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { mail.layout(width: testWidth) } as? ShadowListUtilsInboxRowLayout)
    XCTAssertEqual(mirrored.avatarFrame.minX, testWidth - plain.avatarFrame.maxX)
    XCTAssertEqual(mirrored.unreadDotFrame.minX, testWidth - plain.unreadDotFrame.maxX)
    XCTAssertEqual(mirrored.senderFrame.maxX, testWidth - 80)
    XCTAssertEqual(mirrored.timeFrame.minX, 16)
    XCTAssertEqual(mirrored.flagFrame.minX, mirrored.timeFrame.maxX + 4, accuracy: 1e-9)
  }

  func testTheRowReadsItsStatesAndShowsItsSelectionWhileEditing() {
    let view = ShadowListUtilsInboxRowView(reuseIdentifier: nil)
    let item = message(0, read: false, flagged: true)
    view.configure(row(item), layout: nil)
    XCTAssertEqual(view.accessibilityLabel, "Unread, Flagged, Sender 0, Subject 0, \(item.preview)")
    XCTAssertEqual(view.backgroundColor, theme.colors.background)
    view.setSelected(true, animated: false)
    XCTAssertEqual(view.backgroundColor, theme.colors.background)

    view.configure(row(message(1), editing: true), layout: nil)
    XCTAssertEqual(view.backgroundColor, theme.colors.fill)
    XCTAssertEqual(view.accessibilityLabel, "Selected, Sender 1, Subject 1, \(item.preview)")
    XCTAssertTrue(view.accessibilityTraits.contains(.selected))
    view.setSelected(false, animated: false)
    XCTAssertEqual(view.backgroundColor, theme.colors.background)
    view.setHighlighted(true, animated: false)
    XCTAssertEqual(view.backgroundColor, theme.colors.fill)
  }

  func testTheAgeComesFromTheLabels() {
    XCTAssertEqual(row(message(0)).time, "5m")
    var labels = ShadowListUtilsInboxLabels()
    labels.time.minutes = { "\($0) min ago" }
    XCTAssertEqual(ShadowListUtilsInboxRow(item: message(0), editing: false, theme: theme, labels: labels).time, "5 min ago")
  }
}
