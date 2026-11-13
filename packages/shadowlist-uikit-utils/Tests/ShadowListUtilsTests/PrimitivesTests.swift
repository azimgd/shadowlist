import ShadowListKit
@testable import ShadowListUtils
import ShadowListUtilsCore
import UIKit
import XCTest

/*
 * The primitives, the empty state, the time labels and the calls into the core they rest on.
 */
@MainActor
final class PrimitivesTests: XCTestCase {
  private let theme = ShadowListUtilsTheme.light

  // MARK: - Core

  func testTheCoreAnswersTheAvatarVectors() throws {
    for entry in try Vectors.cases("avatar.json") {
      let name = try XCTUnwrap(entry["name"] as? String)
      // Foundation's JSON parser drops a byte order mark inside a string. That case is written out below.
      if name.contains("byte order mark") { continue }
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      let output = try XCTUnwrap(entry["output"] as? [String: Any])
      // A case with half a surrogate pair lists its UTF-16 units, which not every JSON reader takes in a string.
      let units = (input["units"] as? [Int])?.map { unichar($0) }
      let text = try units.map { NSString(characters: $0, length: $0.count) as String } ?? XCTUnwrap(input["name"] as? String)
      let paletteSize = try XCTUnwrap(input["paletteSize"] as? Int)
      XCTAssertEqual(ShadowListUtilsCore.avatarColorIndex(forName: text, paletteSize: paletteSize), output["colorIndex"] as? Int, name)
      XCTAssertEqual(ShadowListUtilsCore.avatarInitials(forName: text), output["initials"] as? String, name)
    }
  }

  func testTheCoreReadsAByteOrderMarkAsWhiteSpace() {
    XCTAssertEqual(ShadowListUtilsCore.avatarColorIndex(forName: "\u{FEFF}Zoe Quinn", paletteSize: 8), 1)
    XCTAssertEqual(ShadowListUtilsCore.avatarInitials(forName: "\u{FEFF}Zoe Quinn"), "ZQ")
  }

  func testTheCoreKeepsAnAstralCharacterWhole() {
    XCTAssertEqual(ShadowListUtilsCore.avatarInitials(forName: "\u{1F600}mile \u{1D4D0}da"), "\u{1F600}\u{1D4D0}")
    XCTAssertEqual(ShadowListUtilsCore.sectionInitial(forName: "\u{1D4D0}da"), "\u{1D4D0}")
  }

  func testTheCoreReadsHalfASurrogatePairAsTheReplacementCharacter() {
    // A high surrogate with no low one after it, which no UTF-8 string holds. Every platform hashes U+FFFD in its place.
    let units: [unichar] = [0xD83D, 0x41]
    let name = NSString(characters: units, length: units.count)
    XCTAssertEqual(ShadowListUtilsCore.avatarColorIndex(forName: name as String, paletteSize: 1000), (0xFFFD * 31 + 0x41) % 1000)
    XCTAssertEqual(ShadowListUtilsCore.avatarInitials(forName: name as String), "\u{FFFD}")
  }

  func testTheCoreAnswersTheRelativeTimeVectors() throws {
    let units: [String: ShadowListUtilsRelativeTimeUnit] = ["now": .now, "minutes": .minutes, "hours": .hours, "days": .days, "date": .date]
    for entry in try Vectors.cases("relative-time.json") {
      let name = try XCTUnwrap(entry["name"] as? String)
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      let output = try XCTUnwrap(entry["output"] as? [String: Any])
      guard let dateMs = input["dateMs"] as? Double, let nowMs = input["nowMs"] as? Double else { continue }
      let time = ShadowListUtilsCore.relativeTime(forDate: dateMs, now: nowMs)
      XCTAssertEqual(time.unit, units[try XCTUnwrap(output["unit"] as? String)], name)
      XCTAssertEqual(time.count, output["count"] as? Int, name)
    }
  }

  func testTheCoreFramesAFeedImage() {
    XCTAssertEqual(ShadowListUtilsCore.feedSingleImageFrame(forWidth: 1000, height: 1000).aspectRatio, 1)
    XCTAssertEqual(ShadowListUtilsCore.feedSingleImageFrame(forWidth: 600, height: 1600).aspectRatio, 0.75)
    XCTAssertEqual(ShadowListUtilsCore.feedSingleImageFrame(forWidth: 3000, height: 500).aspectRatio, 16.0 / 9.0, accuracy: 1e-9)
    let unsized = ShadowListUtilsCore.feedSingleImageFrame(forWidth: 0, height: 0)
    XCTAssertEqual(unsized.aspectRatio, 0)
    XCTAssertEqual(unsized.height, 200)
  }

  func testTheCoreGivesTheLayoutConstants() {
    XCTAssertEqual(SHADOWLIST_UTILS_CHAT_BUBBLE_PADDING_HORIZONTAL, 14)
    XCTAssertEqual(SHADOWLIST_UTILS_CHAT_BUBBLE_WIDTH_FRACTION, 0.75)
    XCTAssertEqual(SHADOWLIST_UTILS_FEED_SINGLE_IMAGE_HEIGHT, 200)
    XCTAssertEqual(ShadowListUtilsCore.sectionOtherTitle(), "#")
    let metrics = ShadowListUtilsCore.avatarInitialsMetrics(forSize: 40)
    XCTAssertEqual(metrics.fontSize, 17)
    XCTAssertEqual(metrics.lineHeight, 21)
    XCTAssertTrue(ShadowListUtilsCore.relativeTimeHasDate(0))
    XCTAssertFalse(ShadowListUtilsCore.relativeTimeHasDate(.nan))
  }

  // MARK: - Labels

  func testRelativeTimeLabelsWordEachBucket() {
    let labels = ShadowListUtilsRelativeTimeLabels()
    let now = 1_762_905_600_000.0
    XCTAssertEqual(labels.format(dateMs: now - 5_000, nowMs: now), "now")
    XCTAssertEqual(labels.format(dateMs: now - 5 * 60_000, nowMs: now), "5m")
    XCTAssertEqual(labels.format(dateMs: now - 3 * 3_600_000, nowMs: now), "3h")
    XCTAssertEqual(labels.format(dateMs: now - 2 * 86_400_000, nowMs: now), "2d")
    let formatter = DateFormatter()
    formatter.setLocalizedDateFormatFromTemplate("MMMd")
    let old = now - 30 * 86_400_000
    XCTAssertEqual(labels.format(dateMs: old, nowMs: now), formatter.string(from: Date(timeIntervalSince1970: old / 1000)))
  }

  func testRelativeTimeLabelsTakeTheAppsWords() {
    var labels = ShadowListUtilsRelativeTimeLabels()
    labels.now = "just now"
    labels.minutes = { "\($0) min" }
    let now = 1_762_905_600_000.0
    XCTAssertEqual(labels.format(dateMs: now, nowMs: now), "just now")
    XCTAssertEqual(labels.format(dateMs: now - 120_000, nowMs: now), "2 min")
  }

  func testRelativeTimeLabelsWriteNothingForADateThatIsNotFinite() {
    let labels = ShadowListUtilsRelativeTimeLabels()
    let now = 1_762_905_600_000.0
    XCTAssertEqual(labels.format(dateMs: .nan, nowMs: now), "")
    XCTAssertEqual(labels.format(dateMs: -.infinity, nowMs: now), "")
    XCTAssertFalse(labels.format(dateMs: now - 30 * 86_400_000, nowMs: now).isEmpty)
  }

  // MARK: - Views

  func testAnAvatarIsACircleOfItsSizeInThePaletteColorOfTheName() {
    let avatar = ShadowListUtilsAvatar(theme: theme)
    avatar.show(name: "Ada Lovelace")
    XCTAssertEqual(avatar.size, 40)
    XCTAssertEqual(avatar.frame.size, CGSize(width: 40, height: 40))
    XCTAssertEqual(avatar.layer.cornerRadius, 20)
    let index = ShadowListUtilsCore.avatarColorIndex(forName: "Ada Lovelace", paletteSize: theme.colors.avatarPalette.count)
    XCTAssertEqual(avatar.backgroundColor, theme.colors.avatarPalette[index])
    XCTAssertFalse(avatar.isAccessibilityElement)
    avatar.show(name: "Ada Lovelace", color: .purple)
    XCTAssertEqual(avatar.backgroundColor, .purple)
    XCTAssertEqual(ShadowListUtilsAvatar(size: 30, theme: theme).layer.cornerRadius, 15)
  }

  func testAnAvatarWithoutAPaletteTakesTheFillColor() {
    var bare = theme
    bare.colors.avatarPalette = []
    let avatar = ShadowListUtilsAvatar(theme: bare)
    avatar.show(name: "Ada Lovelace")
    XCTAssertEqual(avatar.backgroundColor, bare.colors.fill)
  }

  func testAnAvatarFollowsANewTheme() throws {
    let avatar = ShadowListUtilsAvatar(theme: .light)
    avatar.show(name: "Ada Lovelace")
    var shifted = ShadowListUtilsTheme.dark
    shifted.colors.avatarPalette = Array(shifted.colors.avatarPalette.reversed())
    avatar.theme = shifted
    let index = ShadowListUtilsCore.avatarColorIndex(forName: "Ada Lovelace", paletteSize: shifted.colors.avatarPalette.count)
    XCTAssertEqual(avatar.backgroundColor, shifted.colors.avatarPalette[index])
    let initials = try XCTUnwrap(avatar.subviews.compactMap { $0 as? ShadowListKitTextView }.first)
    XCTAssertEqual(initials.textColor, shifted.colors.label)
  }

  func testAnAvatarsInitialsKeepTheirSizeAtALargeTextSize() throws {
    func initialsSize(_ name: String) throws -> CGSize {
      let avatar = ShadowListUtilsAvatar(theme: theme)
      avatar.show(name: name)
      return try XCTUnwrap(avatar.subviews.compactMap { $0 as? ShadowListKitTextView }.first?.textLayout).size
    }
    let plain = try initialsSize("Grace Hopper")
    XCTAssertEqual(plain.height, 21)
    // Another name, which the avatars have not typeset before.
    XCTAssertEqual(try withTextSize(2) { try initialsSize("Hedy Gamarr") }.height, 21)
  }

  func testAnImageViewFillsClipsAndShowsItsPlaceholderColor() {
    let view = ShadowListUtilsImageView()
    XCTAssertEqual(view.contentMode, .scaleAspectFill)
    XCTAssertTrue(view.clipsToBounds)
    view.cornerRadius = 12
    view.placeholderColor = theme.colors.elevated2
    XCTAssertEqual(view.layer.cornerRadius, 12)
    XCTAssertEqual(view.backgroundColor, theme.colors.elevated2)
    view.placeholderColor = ShadowListUtilsTheme.system.colors.elevated2
    XCTAssertEqual(view.backgroundColor, ShadowListUtilsTheme.system.colors.elevated2)
  }

  func testAnImageViewAsksTheSharedLoaderOncePerUrlWhenItHasASize() {
    final class Recorder: ShadowListUtilsImageLoader {
      var loaded: [String] = []
      var cancelled = 0
      override func load(_ url: String, into view: ShadowListUtilsImageView) { loaded.append(url) }
      override func cancel(_ view: ShadowListUtilsImageView) { cancelled += 1 }
    }
    let previous = ShadowListUtilsImageLoader.shared
    let recorder = Recorder()
    ShadowListUtilsImageLoader.shared = recorder
    defer { ShadowListUtilsImageLoader.shared = previous }
    let view = ShadowListUtilsImageView()
    view.url = "https://example.com/a.jpg"
    XCTAssertEqual(recorder.loaded, [])
    view.frame = CGRect(x: 0, y: 0, width: 100, height: 80)
    view.layoutIfNeeded()
    view.setNeedsLayout()
    view.layoutIfNeeded()
    XCTAssertEqual(recorder.loaded, ["https://example.com/a.jpg"])
    view.url = "https://example.com/b.jpg"
    XCTAssertEqual(recorder.loaded, ["https://example.com/a.jpg", "https://example.com/b.jpg"])
    XCTAssertEqual(recorder.cancelled, 2)
    view.url = nil
    XCTAssertNil(view.image)
  }

  func testAnItemSeparatorIsAHairlineAfterTheRowInset() {
    let separator = ShadowListUtilsItemSeparator(theme: theme)
    XCTAssertEqual(separator.inset, 68)
    XCTAssertEqual(separator.sizeThatFits(CGSize(width: testWidth, height: 100)), CGSize(width: testWidth, height: ShadowListUtilsTheme.hairline))
    separator.frame = CGRect(x: 0, y: 0, width: testWidth, height: ShadowListUtilsTheme.hairline)
    separator.layoutIfNeeded()
    XCTAssertEqual(separator.subviews.first?.frame, CGRect(x: 68, y: 0, width: testWidth - 68, height: ShadowListUtilsTheme.hairline))
    XCTAssertEqual(separator.subviews.first?.backgroundColor, theme.colors.separator)
    XCTAssertEqual(separator.backgroundColor, theme.colors.background)
    separator.theme = .dark
    XCTAssertEqual(separator.backgroundColor, ShadowListUtilsTheme.dark.colors.background)
  }

  func testAListHeaderIsItsTitleAndSubtitleWithTheirPadding() {
    let typography = theme.typography
    let titled = ShadowListUtilsListHeader(title: "Lisbon Crew", theme: theme)
    XCTAssertEqual(titled.sizeThatFits(CGSize(width: testWidth, height: 0)).height, 4 + typography.largeTitle.lineHeight + 12)
    let header = ShadowListUtilsListHeader(title: "Lisbon Crew", subtitle: "8 travellers", theme: theme)
    XCTAssertEqual(
      header.sizeThatFits(CGSize(width: testWidth, height: 0)).height,
      4 + typography.largeTitle.lineHeight + 2 + typography.subhead.lineHeight + 12)
    XCTAssertEqual(header.backgroundColor, theme.colors.background)
    let texts = header.subviews.compactMap { $0 as? ShadowListKitTextView }
    XCTAssertEqual(texts.first?.frame.origin, CGPoint(x: 16, y: 4))
    XCTAssertEqual(texts.last?.frame.origin, CGPoint(x: 16, y: 4 + typography.largeTitle.lineHeight + 2))
    XCTAssertEqual(texts.first?.accessibilityTraits, .header)
  }

  func testAListFooterCentersItsTextWithItsPadding() throws {
    let footer = ShadowListUtilsListFooter(text: "42 travellers", theme: theme)
    XCTAssertEqual(footer.sizeThatFits(CGSize(width: testWidth, height: 0)).height, 20 + theme.typography.footnote.lineHeight + 20)
    let text = try XCTUnwrap(footer.subviews.first as? ShadowListKitTextView)
    XCTAssertEqual(text.frame.midX, testWidth / 2, accuracy: 1)
    XCTAssertEqual(text.frame.minY, 20)
    XCTAssertEqual(text.textColor, theme.colors.secondaryLabel)
  }

  func testASectionHeaderIsOneLineBetweenItsPadding() throws {
    let header = ShadowListUtilsSectionHeader(key: "a", title: "a", count: 12, theme: theme)
    let layout = try XCTUnwrap(header.layout(width: testWidth) as? ShadowListUtilsSectionHeaderLayout)
    XCTAssertEqual(layout.height, 38)
    XCTAssertEqual(layout.titleFrame.origin, CGPoint(x: 16, y: 10))
    XCTAssertEqual(layout.countFrame.maxX, testWidth - 16)
    XCTAssertEqual(layout.countFrame.minY, 10)
    XCTAssertNil((ShadowListUtilsSectionHeader(key: "b", title: "b", theme: theme).layout(width: testWidth) as? ShadowListUtilsSectionHeaderLayout)?.count)
    let view = ShadowListUtilsSectionHeaderView(reuseIdentifier: nil)
    view.configure(header, layout: layout)
    XCTAssertEqual(view.backgroundColor, theme.colors.elevated)
    XCTAssertEqual(view.accessibilityLabel, "a, 12")
    XCTAssertTrue(view.accessibilityTraits.contains(.header))
    XCTAssertNotEqual(header.contentVersion, ShadowListUtilsSectionHeader(key: "a", title: "a", count: 13, theme: theme).contentVersion)
  }

  func testARightToLeftSectionHeaderMirrorsItsFrames() throws {
    let header = ShadowListUtilsSectionHeader(key: "a", title: "a", count: 12, theme: theme)
    let plain = try XCTUnwrap(header.layout(width: testWidth) as? ShadowListUtilsSectionHeaderLayout)
    let mirrored = try XCTUnwrap(withRightToLeft { header.layout(width: testWidth) } as? ShadowListUtilsSectionHeaderLayout)
    XCTAssertEqual(mirrored.titleFrame.maxX, testWidth - 16)
    XCTAssertEqual(mirrored.countFrame.minX, 16)
    XCTAssertEqual(mirrored.titleFrame.size, plain.titleFrame.size)
  }

  func testASpinnerPadsTheSystemIndicatorAndReadsAsLoading() throws {
    let spinner = ShadowListUtilsSpinner(theme: theme)
    XCTAssertEqual(spinner.sizeThatFits(CGSize(width: testWidth, height: 0)), CGSize(width: testWidth, height: 20 + 32))
    let indicator = try XCTUnwrap(spinner.subviews.first as? UIActivityIndicatorView)
    XCTAssertEqual(indicator.color, theme.colors.secondaryLabel)
    XCTAssertEqual(indicator.accessibilityLabel, "Loading")
    XCTAssertTrue(indicator.isAnimating)
    XCTAssertEqual((ShadowListUtilsSpinner(color: .red, theme: theme).subviews.first as? UIActivityIndicatorView)?.color, .red)
  }

  func testRedactedTextIsOneCenteredBarPerLineThatHoldsText() throws {
    let subhead = theme.typography.subhead
    let layout = subhead.layout("One long line of words that wraps.\n\nShort", width: 200)
    XCTAssertGreaterThanOrEqual(layout.lineCount, 4)
    let bars = ShadowListUtilsRedactedText.bars(for: layout, lineHeight: subhead.lineHeight)
    // The blank line has no bar.
    XCTAssertEqual(bars.count, Int(layout.lineCount) - 1)
    for bar in bars {
      let line = (bar.midY / subhead.lineHeight).rounded(.down)
      XCTAssertEqual(bar.height, subhead.lineHeight * 0.62, accuracy: 1e-9)
      XCTAssertEqual(bar.midY, (line + 0.5) * subhead.lineHeight, accuracy: 1e-9)
      XCTAssertEqual(bar.width, layout.rectForLine(at: UInt(line)).width)
    }
    // A bar is as wide as its own line's text, not as the widest line.
    XCTAssertLessThan(try XCTUnwrap(bars.last).width, bars[0].width / 2)
    XCTAssertEqual(bars.map(\.minY), bars.map(\.minY).sorted())
  }

  func testAnEmptyStateStacksItsIconTitleAndMessageInACenteredColumn() {
    let icon = UIView(frame: CGRect(x: 0, y: 0, width: 48, height: 48))
    let state = ShadowListUtilsEmptyState(title: "No messages", message: "New mail shows here.", icon: icon, theme: theme)
    let height = state.sizeThatFits(CGSize(width: testWidth, height: 0)).height
    XCTAssertEqual(height, 28 + 48 + 4 + 6 + theme.typography.body.lineHeight + 4 + theme.typography.footnote.lineHeight + 28)
    XCTAssertEqual(icon.frame, CGRect(x: (testWidth - 48) / 2, y: 28, width: 48, height: 48))
    XCTAssertTrue(state.isAccessibilityElement)
    XCTAssertEqual(state.accessibilityLabel, "No messages. New mail shows here.")
    let titled = ShadowListUtilsEmptyState(title: "No messages", theme: theme)
    XCTAssertEqual(titled.sizeThatFits(CGSize(width: testWidth, height: 0)).height, 28 + 6 + theme.typography.body.lineHeight + 28)
    XCTAssertEqual(titled.accessibilityLabel, "No messages")
  }

  func testTheIconsAreDrawnInsideTheirBox() {
    let box = CGRect(x: 0, y: 0, width: 20, height: 20)
    for kind in [ShadowListUtilsIconView.Kind.grip, .chevron, .check] {
      let path = ShadowListUtilsIconView.path(kind, size: 20)
      XCTAssertFalse(path.isEmpty)
      // The grip is filled. The others are stroked, half their line width outside the path.
      let reach = kind == .grip ? 0 : path.lineWidth / 2
      XCTAssertTrue(box.contains(path.bounds.insetBy(dx: -reach, dy: -reach)), "\(kind)")
    }
    // Three lines as wide as the icon, 1.75 thick and 22 percent of the size apart, centered.
    let grip = ShadowListUtilsIconView.path(.grip, size: 20).bounds
    XCTAssertEqual(grip.width, 20)
    XCTAssertEqual(grip.height, 3 * 1.75 + 2 * 20 * 0.22, accuracy: 1e-9)
    XCTAssertEqual(grip.midY, 10, accuracy: 1e-9)
    // The chevron's tip points right and its arms are 42 percent of the size.
    let chevron = ShadowListUtilsIconView.path(.chevron, size: 20)
    XCTAssertEqual(chevron.lineWidth, 2)
    XCTAssertEqual(chevron.bounds.height, 2 * 20 * 0.42 * 0.5.squareRoot(), accuracy: 1e-4)
    XCTAssertEqual(chevron.bounds.midY, 10, accuracy: 1e-9)
    let check = ShadowListUtilsIconView.path(.check, size: 14)
    XCTAssertEqual(check.lineWidth, 2.2)
    XCTAssertEqual(check.bounds.width, 14 * (0.26 + 0.5) * 0.5.squareRoot(), accuracy: 1e-4)
  }
}
