import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class ContactsTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  private func named(_ names: [String]) -> [ShadowListUtilsContactItem] {
    names.enumerated().map { ShadowListUtilsContactItem(key: "contact-\($0.offset)", name: $0.element) }
  }

  func testTheListShowsItsItems() {
    let list = ShadowListUtilsContactsList(theme: theme)
    host(list)
    list.items = Fixture.contacts(5)
    list.layoutIfNeeded()
    XCTAssertEqual(list.keys, (0..<5).map { "contact-\($0)" })
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsContactRowView)
    XCTAssertFalse(list.allowsSelection)
    XCTAssertTrue(list.disclosureIndicator)
  }

  func testAChangedItemIsConfiguredAgainAndADeletedItemLeaves() {
    let list = ShadowListUtilsContactsList(theme: theme)
    host(list)
    var items = Fixture.contacts(4)
    list.items = items
    list.layoutIfNeeded()
    let untouched = list.rowView(at: 0)
    items[1].subtitle = "Moved to the aisle"
    list.setItems(items, animated: false)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 1)?.row as? ShadowListUtilsContactRow)?.item.subtitle, "Moved to the aisle")
    XCTAssertTrue(list.rowView(at: 0) === untouched)
    items.remove(at: 3)
    list.setItems(items, animated: true)
    XCTAssertEqual(list.keys, ["contact-0", "contact-1", "contact-2"])
  }

  func testOnPressItemRunsForATappedContactAndShowsTheChevron() {
    let list = ShadowListUtilsContactsList(theme: theme)
    host(list)
    list.items = Fixture.contacts(3)
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 0)?.row as? ShadowListUtilsContactRow)?.showsChevron, false)
    var pressed: [String] = []
    list.onPressItem = { pressed.append($0.key) }
    list.layoutIfNeeded()
    XCTAssertTrue(list.allowsSelection)
    XCTAssertEqual((list.rowView(at: 0)?.row as? ShadowListUtilsContactRow)?.showsChevron, true)
    XCTAssertEqual(list.rowView(at: 0)?.accessibilityTraits.contains(.button), true)
    list.tap(1)
    XCTAssertEqual(pressed, ["contact-1"])
    list.disclosureIndicator = false
    list.layoutIfNeeded()
    XCTAssertEqual((list.rowView(at: 0)?.row as? ShadowListUtilsContactRow)?.showsChevron, false)
  }

  func testOnDeleteAddsOneDestructiveTrailingActionInTheThemesRed() throws {
    let list = ShadowListUtilsContactsList(theme: theme)
    host(list)
    list.items = Fixture.contacts(3)
    list.layoutIfNeeded()
    XCTAssertTrue(list.trailingActions(at: 1).isEmpty)
    XCTAssertTrue(list.leadingActions(at: 1).isEmpty)
    var deleted: [String] = []
    list.onDelete = { deleted.append($0.key) }
    let actions = list.trailingActions(at: 1)
    XCTAssertEqual(actions.map(\.title), ["Delete"])
    XCTAssertEqual(actions.first?.style, .destructive)
    XCTAssertEqual(actions.first?.backgroundColor, theme.colors.red)
    actions.first?.perform()
    XCTAssertEqual(deleted, ["contact-1"])
    let reader = try XCTUnwrap(list.rowView(at: 2)?.accessibilityCustomActions)
    XCTAssertEqual(reader.map(\.name), ["Delete"])
    XCTAssertEqual(reader[0].actionHandler?(reader[0]), true)
    XCTAssertEqual(deleted, ["contact-1", "contact-2"])
  }

  func testTheRowLayoutHoldsItsMetrics() throws {
    let row = ShadowListUtilsContactRow(item: Fixture.contact(0), showsChevron: true, theme: theme)
    let layout = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsContactRowLayout)
    XCTAssertEqual(layout.height, 67)
    XCTAssertEqual(layout.avatarFrame, CGRect(x: 16, y: ShadowListUtilsPlatform.roundToPixel(13.5), width: 40, height: 40))
    XCTAssertEqual(layout.nameFrame.origin, CGPoint(x: 68, y: 12))
    XCTAssertEqual(layout.subtitleFrame.origin, CGPoint(x: 68, y: 12 + theme.typography.body.scaledLineHeight() + 1))
    XCTAssertEqual(layout.chevronFrame, CGRect(x: testWidth - 12 - 20, y: ShadowListUtilsPlatform.roundToPixel(23.5), width: 20, height: 20))
    XCTAssertEqual(layout.separatorFrame, CGRect(x: 68, y: 67 - ShadowListUtilsTheme.hairline, width: testWidth - 68, height: ShadowListUtilsTheme.hairline))
    XCTAssertEqual(layout.name.lineCount, 1)

    var plain = Fixture.contact(1)
    plain.subtitle = nil
    let short = try XCTUnwrap(
      ShadowListUtilsContactRow(item: plain, showsChevron: false, theme: theme).layout(width: testWidth) as? ShadowListUtilsContactRowLayout)
    XCTAssertEqual(short.height, 40 + 24)
    XCTAssertNil(short.subtitle)
    XCTAssertEqual(short.chevronFrame, .zero)
  }

  func testANameWrapsAtALargeTextSize() throws {
    var contact = Fixture.contact(0)
    contact.name = String(repeating: "Bartholomew ", count: 6)
    let row = ShadowListUtilsContactRow(item: contact, showsChevron: false, theme: theme)
    let plain = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsContactRowLayout)
    XCTAssertEqual(plain.name.lineCount, 1)
    XCTAssertEqual(plain.height, 67)
    let large = try XCTUnwrap(withTextSize(1.5) { row.layout(width: testWidth) } as? ShadowListUtilsContactRowLayout)
    XCTAssertGreaterThan(large.name.lineCount, 1)
    XCTAssertEqual(large.subtitleFrame.minY, large.nameFrame.maxY + 1)
    XCTAssertEqual(large.height, large.subtitleFrame.maxY + 12)
    withTextSize(1.5) { assertLines(large.name, theme.typography.body) }
  }

  func testARightToLeftRowMirrorsItsFrames() throws {
    let row = ShadowListUtilsContactRow(item: Fixture.contact(0), showsChevron: true, theme: theme)
    let plain = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsContactRowLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { row.layout(width: testWidth) } as? ShadowListUtilsContactRowLayout)
    XCTAssertEqual(mirrored.avatarFrame.minX, testWidth - plain.avatarFrame.maxX)
    XCTAssertEqual(mirrored.nameFrame.maxX, testWidth - 68)
    XCTAssertEqual(mirrored.chevronFrame.minX, 12)
    XCTAssertEqual(mirrored.separatorFrame, CGRect(x: 0, y: plain.separatorFrame.minY, width: testWidth - 68, height: plain.separatorFrame.height))
    // The chevron of a layout that runs from right to left points left.
    XCTAssertEqual(
      ShadowListUtilsIconView.path(.chevron, size: 20, isRightToLeft: true).bounds.midX,
      20 - ShadowListUtilsIconView.path(.chevron, size: 20).bounds.midX, accuracy: 0.001)
  }

  func testAPressedRowHighlightsInTheFillColorAtOnce() {
    let view = ShadowListUtilsContactRowView(reuseIdentifier: nil)
    view.configure(ShadowListUtilsContactRow(item: Fixture.contact(0), showsChevron: false, theme: theme), layout: nil)
    XCTAssertEqual(view.backgroundColor, theme.colors.background)
    view.setHighlighted(true, animated: true)
    XCTAssertEqual(view.backgroundColor, theme.colors.fill)
    XCTAssertNil(view.layer.animation(forKey: "backgroundColor"))
    view.setHighlighted(false, animated: true)
    XCTAssertEqual(view.backgroundColor, theme.colors.background)
    XCTAssertEqual(view.accessibilityLabel, "Person 0, Seat 0")
  }

  // MARK: - Sections

  func testGroupIntoSectionsGroupsByTheFirstLetterWithTheRestLast() {
    let sections = shadowListUtilsGroupIntoSections(named(["bob", "Ada", "  Bea", "42", "", "alan", "Émile", "_x"]))
    XCTAssertEqual(sections.map(\.title), ["A", "B", "É", "#"])
    XCTAssertEqual(sections.map(\.key), sections.map(\.title))
    XCTAssertEqual(sections[0].items.map(\.name), ["Ada", "alan"])
    XCTAssertEqual(sections[1].items.map(\.name), ["bob", "  Bea"])
    XCTAssertEqual(sections[3].items.map(\.name), ["42", "", "_x"])
    XCTAssertTrue(shadowListUtilsGroupIntoSections([]).isEmpty)
  }

  func testGroupIntoSectionsTitlesByTheFirstGraphemeCluster() {
    // A letter with a combining accent is one character, and so is a flag, which is no letter.
    let sections = shadowListUtilsGroupIntoSections(named(["e\u{301}mile", "\u{1F1EB}\u{1F1F7} France", "ßen", "\u{FEFF}zoe"]))
    XCTAssertEqual(sections.map(\.title), ["E\u{301}", "SS", "Z", "#"])
    XCTAssertEqual(sections[3].items.map(\.name), ["\u{1F1EB}\u{1F1F7} France"])
  }

  /*
   * group-into-sections.json is a list of cases: input.names in, output.sections out as
   * { title, names }.
   */
  func testGroupIntoSectionsMatchesTheVectors() throws {
    for entry in try Vectors.cases("group-into-sections.json") {
      let name = try XCTUnwrap(entry["name"] as? String)
      let names = try XCTUnwrap((entry["input"] as? [String: Any])?["names"] as? [String], name)
      let expected = try XCTUnwrap((entry["output"] as? [String: Any])?["sections"] as? [[String: Any]], name)
      let sections = shadowListUtilsGroupIntoSections(named(names))
      XCTAssertEqual(sections.map(\.title), expected.map { $0["title"] as? String }, name)
      XCTAssertEqual(sections.map { $0.items.map(\.name) }, expected.map { $0["names"] as? [String] ?? [] }, name)
    }
  }

  func testTheSectionListShowsStickyHeadersWithTheirCounts() throws {
    let list = ShadowListUtilsContactsSectionList(theme: theme)
    XCTAssertTrue(list.stickySectionHeaders)
    host(list)
    list.sections = shadowListUtilsGroupIntoSections(named(["Ada", "Alan", "Bea", "Cy", "Cleo", "Cora"]))
    list.layoutIfNeeded()
    XCTAssertEqual(list.numberOfSections, 3)
    XCTAssertEqual(list.items.map(\.name), ["Ada", "Alan", "Bea", "Cy", "Cleo", "Cora"])
    XCTAssertEqual(list.firstItemIndexInSection(2), 3)
    XCTAssertEqual(list.rectForHeader(inSection: 0), CGRect(x: 0, y: 0, width: testWidth, height: 38))
    XCTAssertEqual(list.rectForItem(at: 0).minY, 38)
    let headers = list.subviews.compactMap { $0 as? ShadowListUtilsSectionHeaderView }.filter { !$0.isHidden }
    XCTAssertEqual(headers.compactMap(\.accessibilityLabel).sorted(), ["A, 2", "B, 1", "C, 3"])
    XCTAssertTrue(list.rowView(at: 0) is ShadowListUtilsContactRowView)
  }

  func testSettingItemsOnTheSectionListGroupsThem() {
    let list = ShadowListUtilsContactsSectionList(theme: theme)
    host(list)
    list.items = named(["Cy", "Ada", "Bea", "Alan"])
    list.layoutIfNeeded()
    XCTAssertEqual(list.sections.map(\.title), ["A", "B", "C"])
    XCTAssertEqual(list.items.map(\.name), ["Ada", "Alan", "Bea", "Cy"])
    XCTAssertEqual(list.keys, ["contact-1", "contact-3", "contact-2", "contact-0"])
  }

  func testSettingSectionsKeepsThemAsGiven() {
    let list = ShadowListUtilsContactsSectionList(theme: theme)
    host(list)
    let contacts = named(["Ada", "Bea", "Cy"])
    list.sections = [
      ShadowListUtilsContactSection(key: "late", title: "Late", items: [contacts[2]]),
      ShadowListUtilsContactSection(key: "early", title: "Early", items: [contacts[0], contacts[1]]),
    ]
    list.layoutIfNeeded()
    XCTAssertEqual(list.sections.map(\.key), ["late", "early"])
    XCTAssertEqual(list.items.map(\.name), ["Cy", "Ada", "Bea"])
    XCTAssertEqual(list.numberOfSections, 2)
    XCTAssertEqual(list.firstItemIndexInSection(1), 1)
  }

  func testASectionsHeaderFollowsItsCount() {
    let list = ShadowListUtilsContactsSectionList(theme: theme)
    host(list)
    var contacts = named(["Ada", "Alan", "Bea"])
    list.sections = shadowListUtilsGroupIntoSections(contacts)
    list.layoutIfNeeded()
    contacts.append(ShadowListUtilsContactItem(key: "contact-9", name: "Abel"))
    list.setSections(shadowListUtilsGroupIntoSections(contacts), animated: false)
    list.layoutIfNeeded()
    let headers = list.subviews.compactMap { $0 as? ShadowListUtilsSectionHeaderView }.filter { !$0.isHidden }
    XCTAssertEqual(headers.compactMap(\.accessibilityLabel).sorted(), ["A, 3", "B, 1"])
  }

  func testANewThemeReachesTheSectionHeaders() {
    let list = ShadowListUtilsContactsSectionList(theme: .light)
    host(list)
    list.sections = shadowListUtilsGroupIntoSections(named(["Ada", "Bea"]))
    list.layoutIfNeeded()
    list.theme = .dark
    list.layoutIfNeeded()
    let headers = list.subviews.compactMap { $0 as? ShadowListUtilsSectionHeaderView }.filter { !$0.isHidden }
    XCTAssertEqual(headers.count, 2)
    for header in headers {
      XCTAssertEqual((header.row as? ShadowListUtilsSectionHeader)?.theme, .dark)
      XCTAssertEqual(header.backgroundColor, ShadowListUtilsTheme.dark.colors.elevated)
    }
    XCTAssertEqual((list.rowView(at: 0)?.row as? ShadowListUtilsContactRow)?.theme, .dark)
  }

  func testTheSectionListsClosuresAndIndexTitles() {
    let list = ShadowListUtilsContactsSectionList(theme: theme)
    host(list)
    list.sections = shadowListUtilsGroupIntoSections(named(["Ada", "Bea", "Cy"]))
    var pressed: [String] = []
    var deleted: [String] = []
    list.onPressItem = { pressed.append($0.name) }
    list.onDelete = { deleted.append($0.name) }
    list.tap(1)
    list.trailingActions(at: 2).first?.perform()
    XCTAssertEqual(pressed, ["Bea"])
    XCTAssertEqual(deleted, ["Cy"])
    XCTAssertNil(list.dataSource?.sectionIndexTitles?(for: list) ?? nil)
    list.sectionIndexTitles = ["A", "B", "C"]
    XCTAssertEqual(list.dataSource?.sectionIndexTitles?(for: list) ?? nil, ["A", "B", "C"])
  }
}
