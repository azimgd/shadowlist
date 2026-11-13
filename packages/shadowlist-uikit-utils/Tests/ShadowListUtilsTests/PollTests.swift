import ShadowListKit
@testable import ShadowListUtils
import ShadowListUtilsCore
import UIKit
import XCTest

@MainActor
final class PollTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light
  private let options = [ShadowListUtilsPollOption(key: "one", label: "Window seat", votes: 1),
                         ShadowListUtilsPollOption(key: "two", label: "Aisle seat", votes: 7)]

  func testTheDefaultChipUsesTheFirstUnicodeScalarAndAnIconChangesTheRowVersion() throws {
    for (text, expected) in [("Lisbon", "L"), ("éclair", "É"), ("e\u{301}clair", "E"), ("👩‍✈️ crew", "👩")] {
      let row = ShadowListUtilsPollRow(item: ShadowListUtilsPollOption(key: "one", label: text, votes: 1), totalVotes: 1, theme: theme)
      let layout = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsPollRowLayout)
      let expectedLayout = theme.typography.headline.layout(expected, width: 44, maxLines: 1, scaled: true, alignment: .center)
      func pixels(_ layout: ShadowListKitTextLayout) -> Data? {
        UIGraphicsImageRenderer(size: CGSize(width: 44, height: 44)).image { context in
          layout.draw(at: .zero, in: context.cgContext, color: UIColor.black.cgColor)
        }.pngData()
      }
      XCTAssertEqual(pixels(layout.initial), pixels(expectedLayout), text)
      XCTAssertEqual(layout.selectionFrame.width, 44)
    }
    let first = ShadowListUtilsPollRow(item: options[0], totalVotes: 8, theme: theme)
    var item = options[0]
    item.icon = UIImage(systemName: "airplane")
    let second = ShadowListUtilsPollRow(item: item, totalVotes: 8, theme: theme)
    XCTAssertNotEqual(first.contentVersion, second.contentVersion)
  }

  func testTheCoreAnswersEverySharedPollVector() throws {
    for entry in try Vectors.cases("poll.json") {
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      let output = try XCTUnwrap(entry["output"] as? [String: Any])
      let votes = try XCTUnwrap(input["votes"] as? [NSNumber])
      let tally = ShadowListUtilsCore.pollTally(forVotes: votes)
      XCTAssertEqual(tally.totalVotes, try XCTUnwrap(output["totalVotes"] as? Double))
      let expected = try XCTUnwrap(output["leadingIndex"] as? Int)
      XCTAssertEqual(tally.leadingIndex, expected < 0 ? NSNotFound : expected)
      let percents = try XCTUnwrap(output["percents"] as? [Int])
      for (index, vote) in votes.enumerated() {
        XCTAssertEqual(ShadowListUtilsCore.pollPercent(forVotes: vote.doubleValue, totalVotes: tally.totalVotes), percents[index])
      }
    }
  }

  func testInvalidVotesUseTheSameCoreResultWithoutPlatformSanitizing() {
    let empty = ShadowListUtilsCore.pollTally(forVotes: [])
    XCTAssertEqual(empty.leadingIndex, NSNotFound)
    let invalid = ShadowListUtilsCore.pollTally(forVotes: [NSNumber(value: Double.nan), 1])
    XCTAssertTrue(invalid.totalVotes.isNaN)
    XCTAssertEqual(invalid.leadingIndex, 0)
    XCTAssertEqual(ShadowListUtilsCore.pollPercent(forVotes: 1, totalVotes: invalid.totalVotes), 0)
    let negative = ShadowListUtilsCore.pollTally(forVotes: [-2, 1])
    XCTAssertEqual(negative.totalVotes, -1)
    XCTAssertEqual(negative.leadingIndex, 1)
  }

  func testPressingAnOptionIsControlledByTheApplication() throws {
    let list = ShadowListUtilsPollList(theme: theme)
    list.poll = ShadowListUtilsPollData(question: "Where do you sit?", options: options)
    host(list)
    XCTAssertFalse(list.allowsSelection)
    XCTAssertEqual(list.keys, ["one", "two"])
    var pressed: [String] = []
    list.onPressItem = { pressed.append($0.key) }
    list.tap(0)
    XCTAssertEqual(pressed, ["one"])
    XCTAssertNil(list.poll.selectedKey)
    XCTAssertEqual(list.poll.options, options)
    var updated = list.poll
    updated.selectedKey = "one"
    updated.options[0].votes = 7
    list.setPoll(updated, animated: false)
    list.layoutIfNeeded()
    let first = try XCTUnwrap(list.rowView(at: 0)?.row as? ShadowListUtilsPollRow)
    let second = try XCTUnwrap(list.rowView(at: 1)?.row as? ShadowListUtilsPollRow)
    XCTAssertTrue(first.isSelected)
    XCTAssertTrue(first.isLeading)
    XCTAssertEqual(first.percent, 50)
    XCTAssertEqual(second.percent, 50)
    list.onPressItem = nil
    XCTAssertFalse(list.allowsSelection)
  }

  func testLabelsAndDefaultQuestionAndTotalsFollowTheModelButCustomViewsStay() throws {
    let list = ShadowListUtilsPollList(theme: theme)
    list.labels.totalVotes = { "Total \($0)" }
    list.labels.option = { "\($0) has \($1) percent" }
    list.poll = ShadowListUtilsPollData(question: "A question", options: options, selectedKey: "two")
    host(list)
    XCTAssertEqual((list.headerView as? ShadowListUtilsListHeader)?.title, "A question")
    let totalFooter = try XCTUnwrap(list.footerView as? ShadowListUtilsPollFooter)
    XCTAssertEqual(totalFooter.subviews.compactMap { $0 as? ShadowListUtilsListFooter }.first?.text, "Total 8.0")
    XCTAssertEqual(list.rowView(at: 0)?.accessibilityLabel, "Window seat has 13 percent")
    XCTAssertTrue(try XCTUnwrap(list.rowView(at: 1)).accessibilityTraits.contains(.selected))
    let header = UIView()
    let footer = UIView()
    list.headerView = header
    list.footerView = footer
    list.poll = ShadowListUtilsPollData(question: "Changed", options: [])
    XCTAssertTrue(list.headerView === header)
    XCTAssertTrue(list.footerView === footer)
    XCTAssertTrue(list.rows.isEmpty)
  }

  func testClearingTheDefaultHeaderAndFooterKeepsThemAbsent() {
    let list = ShadowListUtilsPollList(theme: theme)
    list.poll = ShadowListUtilsPollData(question: "A question", options: options)
    list.headerView = nil
    list.footerView = nil
    list.poll = ShadowListUtilsPollData(question: "Changed", options: options, selectedKey: "one")
    list.labels = ShadowListUtilsPollLabels()
    XCTAssertNil(list.headerView)
    XCTAssertNil(list.footerView)
  }

  func testARightToLeftPollMirrorsItsFramesAndLongLabelsWrap() throws {
    let row = ShadowListUtilsPollRow(item: options[0], totalVotes: 8, theme: theme)
    let regular = try XCTUnwrap(row.layout(width: testWidth) as? ShadowListUtilsPollRowLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { row.layout(width: testWidth) } as? ShadowListUtilsPollRowLayout)
    XCTAssertEqual(regular.barFillFrame.width / regular.barFrame.width, 0.13, accuracy: 0.001)
    XCTAssertEqual(regular.barFillFrame.minX, testWidth - mirrored.barFillFrame.maxX, accuracy: 0.001)
    let long = ShadowListUtilsPollOption(key: "long", label: String(repeating: "A longer option ", count: 10), votes: 1)
    let large = try XCTUnwrap(withTextSize(1.6) {
      ShadowListUtilsPollRow(item: long, totalVotes: 8, theme: theme).layout(width: testWidth)
    } as? ShadowListUtilsPollRowLayout)
    XCTAssertGreaterThan(large.height, regular.height)
    XCTAssertGreaterThan(large.label.lineCount, 1)
  }
}
