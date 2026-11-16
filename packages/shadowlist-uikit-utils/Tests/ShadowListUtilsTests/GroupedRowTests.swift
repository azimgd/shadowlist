import ShadowListKit
@testable import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class GroupedRowTests: XCTestCase {
  private func attached(_ row: UIView, width: CGFloat = 260) -> UIWindow {
    let window = UIWindow(frame: CGRect(x: 0, y: 0, width: 320, height: 600))
    let controller = UIViewController()
    window.rootViewController = controller
    controller.view.addSubview(row)
    row.frame = CGRect(origin: .zero, size: row.sizeThatFits(CGSize(width: width, height: CGFloat.greatestFiniteMagnitude)))
    window.isHidden = false
    row.layoutIfNeeded()
    return window
  }
  private func itemActions(_ view: UIView) -> ShadowListKitItemActionsView? {
    if let value = view as? ShadowListKitItemActionsView { return value }
    return view.subviews.compactMap(itemActions).first
  }
  private func labels(_ view: UIView) -> [UILabel] {
    view.subviews.flatMap { child in (child as? UILabel).map { [$0] } ?? labels(child) }
  }

  func testOptionalPrimaryPressKeepsRoleAndControlledLabel() {
    let row = ShadowListUtilsGroupedRow(title: "Reservation", theme: .light)
    XCTAssertEqual(row.alpha, 1)
    XCTAssertEqual(row.accessibilityLabel, "Reservation")
    XCTAssertTrue(row.accessibilityTraits.contains(.button))
    XCTAssertFalse(row.accessibilityTraits.contains(.notEnabled))
    XCTAssertTrue(row.accessibilityActivate())
    row.subtitle = "Kyoto"
    row.testID = "reservation"
    row.accessibilityRole = .link
    XCTAssertEqual(row.accessibilityLabel, "Reservation, Kyoto")
    XCTAssertEqual(row.accessibilityIdentifier, "reservation")
    XCTAssertTrue(row.accessibilityTraits.contains(.link))
  }

  func testPhysicalTextHitReachesPrimaryButtonAndPreservesSlotControls() throws {
    let row = ShadowListUtilsGroupedRow(title: "Reservation")
    var presses = 0
    row.onPress = { presses += 1 }
    let window = attached(row)
    defer { window.isHidden = true }
    let hit = try XCTUnwrap(row.hitTest(CGPoint(x: 100, y: row.bounds.midY), with: nil) as? UIButton)
    hit.sendActions(for: .touchUpInside)
    XCTAssertEqual(presses, 1)

    let trailing = UIButton(type: .system)
    trailing.setTitle("More", for: .normal)
    row.trailing = trailing
    row.layoutIfNeeded()
    window.layoutIfNeeded()
    let point = row.convert(CGPoint(x: trailing.bounds.midX, y: trailing.bounds.midY), from: trailing)
    XCTAssertTrue(row.hitTest(point, with: nil) === trailing)
  }

  func testDisabledPrimaryAndLabelledAccessibilityHoldKeepIndependentActions() throws {
    let row = ShadowListUtilsGroupedRow(title: "Reservation")
    var presses = 0; var holds = 0; var archives = 0; var caller = 0
    row.onPress = { presses += 1 }; row.onLongPress = { holds += 1 }; row.longPressLabel = "Details"
    let own = UIAccessibilityCustomAction(name: "Caller") { _ in caller += 1; return true }
    row.accessibilityCustomActions = [own]
    row.trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(actions: [
      ShadowListKitSwipeAction(style: .normal, title: "Archive") { _, done in archives += 1; done(true) },
    ])
    let window = attached(row); defer { window.isHidden = true }
    row.disabled = true
    XCTAssertFalse(row.accessibilityActivate()); XCTAssertEqual(presses, 0)
    XCTAssertTrue(row.accessibilityTraits.contains(.notEnabled))
    XCTAssertTrue(row.isAccessibilityElement)
    let actions = try XCTUnwrap(row.accessibilityCustomActions)
    let details = try XCTUnwrap(actions.first { $0.name == "Details" })
    let archive = try XCTUnwrap(actions.first { $0.name == "Archive" })
    // RN's labelled custom action handler is independent of Pressable's physical disabled gate.
    XCTAssertEqual(details.actionHandler?(details), true)
    XCTAssertEqual(archive.actionHandler?(archive), true)
    XCTAssertEqual(own.actionHandler?(own), true)
    XCTAssertEqual(holds, 1); XCTAssertEqual(archives, 1); XCTAssertEqual(caller, 1)
  }

  func testNativeSlotsAndLargeTextPreserveCallerStateAndGrowTheRow() {
    let row = ShadowListUtilsGroupedRow(title: "A long reservation title that wraps at a narrow width", theme: .light)
    row.subtitle = "A second line with an address and arrival information"
    let leading = UIButton(type: .system); leading.setTitle("Map", for: .normal)
    leading.isAccessibilityElement = true
    leading.transform = CGAffineTransform(scaleX: 0.8, y: 0.8)
    let trailing = UIButton(type: .system); trailing.setTitle("More", for: .normal)
    row.leading = leading; row.trailing = trailing; row.separated = true
    let window = attached(row, width: 220); defer { window.isHidden = true }
    let normal = row.sizeThatFits(CGSize(width: 220, height: CGFloat.greatestFiniteMagnitude)).height
    XCTAssertGreaterThanOrEqual(normal, row.theme.grouped.rowHeight)
    let previous = ShadowListUtilsPlatform.textScaleOverride
    ShadowListUtilsPlatform.textScaleOverride = 2
    defer { ShadowListUtilsPlatform.textScaleOverride = previous }
    row.subtitleLines = 1
    let large = row.sizeThatFits(CGSize(width: 220, height: CGFloat.greatestFiniteMagnitude)).height
    XCTAssertGreaterThan(large, normal)
    XCTAssertTrue(labels(row).filter { $0.text?.contains("reservation") == true }.allSatisfy { $0.numberOfLines == 3 })
    XCTAssertTrue(leading.isUserInteractionEnabled); XCTAssertTrue(leading.isAccessibilityElement)
    XCTAssertEqual(leading.transform, CGAffineTransform(scaleX: 0.8, y: 0.8))
    XCTAssertTrue(trailing.isUserInteractionEnabled)
  }

  func testGroupedThemingAlwaysCopiesFromOriginalConfiguration() throws {
    let implicit = ShadowListKitSwipeAction(style: .destructive, title: "Remove") { _, _ in }
    let explicit = ShadowListKitSwipeAction(style: .destructive, title: "Explicit") { _, _ in }
    explicit.backgroundColor = .systemRed
    let transparent = ShadowListKitSwipeAction(style: .destructive, title: "Clear") { _, _ in }
    transparent.backgroundColor = .clear
    let configuration = ShadowListKitSwipeActionsConfiguration(actions: [implicit, explicit, transparent])
    configuration.performsFirstActionWithFullSwipe = false
    let wrapper = ShadowListUtilsGroupedActions(children: [UIView()], theme: .light)
    wrapper.trailingSwipeActions = configuration
    let native = try XCTUnwrap(itemActions(wrapper))
    var theme = ShadowListUtilsTheme.light; theme.colors.red = .purple
    wrapper.theme = theme
    XCTAssertEqual(native.trailingSwipeActions?.actions.first?.backgroundColor, .purple)
    XCTAssertTrue(native.trailingSwipeActions?.actions[1] === explicit)
    XCTAssertTrue(native.trailingSwipeActions?.actions[2] === transparent)
    XCTAssertEqual(implicit.backgroundColor, .systemRed)
    XCTAssertFalse(try XCTUnwrap(native.trailingSwipeActions).performsFirstActionWithFullSwipe)
    theme.colors.red = .orange; wrapper.theme = theme
    XCTAssertEqual(native.trailingSwipeActions?.actions.first?.backgroundColor, .orange)
    XCTAssertEqual(configuration.actions.count, 3)
  }
}
