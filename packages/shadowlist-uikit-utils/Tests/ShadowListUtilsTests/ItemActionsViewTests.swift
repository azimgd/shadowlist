import ShadowListKit
import ShadowListUtils
import UIKit
import XCTest

@MainActor
final class ItemActionsViewTests: XCTestCase {
  private final class Animator: NSObject, UIContextMenuInteractionAnimating {
    var previewViewController: UIViewController? { nil }
    func addAnimations(_ animations: @escaping () -> Void) { animations() }
    func addCompletion(_ completion: @escaping () -> Void) { completion() }
  }

  private func attached(_ view: UIView) -> UIWindow {
    let window = UIWindow(frame: CGRect(x: 0, y: 0, width: 320, height: 480))
    let controller = UIViewController()
    window.rootViewController = controller
    controller.view.addSubview(view)
    view.frame = CGRect(x: 10, y: 20, width: 300, height: 60)
    window.isHidden = false
    view.setNeedsLayout(); view.layoutIfNeeded()
    return window
  }

  private func configuration(_ title: String, full: Bool = false,
    handler: @escaping (ShadowListKitSwipeAction, @escaping (Bool) -> Void) -> Void
  ) -> ShadowListKitSwipeActionsConfiguration {
    let configuration = ShadowListKitSwipeActionsConfiguration(actions: [
      ShadowListKitSwipeAction(style: .normal, title: title, handler: handler),
    ])
    configuration.performsFirstActionWithFullSwipe = full
    return configuration
  }

  func testRevealMovesOnlyTheLibrarySurfaceAndPreservesContentIdentity() throws {
    let view = ShadowListKitItemActionsView()
    let content = view.contentView
    let button = UIButton(type: .system)
    button.transform = CGAffineTransform(scaleX: 0.8, y: 1.1)
    content.addSubview(button)
    view.leadingSwipeActions = configuration("Archive") { _, _ in }
    let window = attached(view)
    defer { window.isHidden = true }
    let surface = try XCTUnwrap(content.superview)
    view.shadowListKit_swipeDistance(100, velocity: 0)
    XCTAssertGreaterThan(surface.transform.tx, 0)
    XCTAssertTrue(view.contentView === content)
    XCTAssertEqual(content.transform, .identity)
    XCTAssertEqual(button.transform, CGAffineTransform(scaleX: 0.8, y: 1.1))
    view.closeSwipeActions(animated: false)
    XCTAssertEqual(surface.transform, .identity)
    XCTAssertTrue(button.superview === content)
  }

  func testNativeActionButtonUsesTheConfiguredHandlerAndFullSwipeUsesTheSamePath() throws {
    let view = ShadowListKitItemActionsView()
    var titles: [String] = []
    view.trailingSwipeActions = configuration("Delete", full: true) { action, completion in
      titles.append(action.title ?? ""); completion(true)
    }
    let window = attached(view)
    defer { window.isHidden = true }
    view.shadowListKit_swipeDistance(-80, velocity: 0)
    let buttons = view.subviews.flatMap { $0.subviews }.compactMap { $0 as? UIButton }
    let button = try XCTUnwrap(buttons.first { $0.currentTitle == "Delete" })
    button.sendActions(for: .touchUpInside)
    XCTAssertEqual(titles, ["Delete"])
    view.closeSwipeActions(animated: false)
    view.shadowListKit_swipeDistance(-500, velocity: 0)
    XCTAssertEqual(titles, ["Delete", "Delete"])
  }

  func testOldAsyncCompletionCannotCloseAReplacementSession() async throws {
    let view = ShadowListKitItemActionsView()
    var finish: ((Bool) -> Void)?
    view.leadingSwipeActions = configuration("First", full: true) { _, completion in finish = completion }
    let window = attached(view)
    defer { window.isHidden = true }
    view.shadowListKit_swipeDistance(500, velocity: 0)
    let completion = try XCTUnwrap(finish)
    view.leadingSwipeActions = configuration("Second") { _, _ in }
    view.shadowListKit_swipeDistance(100, velocity: 0)
    let surface = try XCTUnwrap(view.contentView.superview)
    let resting = surface.transform
    XCTAssertGreaterThan(resting.tx, 0)
    completion(true)
    let drained = expectation(description: "completion dispatched")
    DispatchQueue.main.async { drained.fulfill() }
    await fulfillment(of: [drained], timeout: 1)
    XCTAssertEqual(surface.transform, resting)
  }

  func testAccessibilityActionsAppendToTheExposedOwnerAndResolveCurrentHandlers() throws {
    let view = ShadowListKitItemActionsView()
    view.isAccessibilityElement = true
    view.accessibilityLabel = "Flight"
    view.accessibilityTraits = .button
    let own = UIAccessibilityCustomAction(name: "Details") { _ in true }
    view.accessibilityCustomActions = [own]
    var pressed: [String] = []
    view.leadingSwipeActions = configuration("Archive") { _, _ in pressed.append("first") }
    let window = attached(view)
    defer { window.isHidden = true }
    let actions = try XCTUnwrap(view.accessibilityCustomActions)
    XCTAssertEqual(actions.map(\.name), ["Details", "Archive"])
    XCTAssertTrue(actions[0] === own)
    view.leadingSwipeActions = configuration("Archive") { _, _ in pressed.append("current") }
    XCTAssertEqual(actions[1].actionHandler?(actions[1]), true)
    XCTAssertEqual(pressed, ["current"])
    XCTAssertEqual(view.accessibilityLabel, "Flight")
    XCTAssertEqual(view.accessibilityTraits, .button)
    let later = UIAccessibilityCustomAction(name: "Later") { _ in true }
    view.accessibilityCustomActions = [own, later]
    view.removeFromSuperview()
    XCTAssertEqual(actions[1].actionHandler?(actions[1]), false)
    XCTAssertEqual(pressed, ["current"])
    XCTAssertEqual(view.accessibilityCustomActions?.map(\.name), ["Details", "Later"])
    XCTAssertEqual(view.accessibilityLabel, "Flight")
    XCTAssertEqual(view.accessibilityTraits, .button)
  }

  func testDetachRestoresSurfaceAndReattachCanOpenAgain() throws {
    let view = ShadowListKitItemActionsView()
    view.leadingSwipeActions = configuration("Archive") { _, _ in }
    let window = attached(view)
    defer { window.isHidden = true }
    let surface = try XCTUnwrap(view.contentView.superview)
    view.shadowListKit_swipeDistance(100, velocity: 0)
    XCTAssertGreaterThan(surface.transform.tx, 0)
    view.removeFromSuperview()
    XCTAssertEqual(surface.transform, .identity)
    window.rootViewController?.view.addSubview(view)
    view.layoutIfNeeded()
    view.shadowListKit_swipeDistance(100, velocity: 0)
    XCTAssertGreaterThan(surface.transform.tx, 0)
  }

  func testNativeContextMenuOwnsPresentationAndDeclinesWhileSwiped() throws {
    let view = ShadowListKitItemActionsView()
    view.contextMenu = UIMenu(children: [UIAction(title: "Inspect") { _ in }])
    var presentations = 0
    view.onContextMenuWillShow = { presentations += 1 }
    let window = attached(view)
    defer { window.isHidden = true }
    let interaction = try XCTUnwrap(view.interactions.compactMap { $0 as? UIContextMenuInteraction }.first)
    let configuration = try XCTUnwrap(interaction.delegate?.contextMenuInteraction(interaction,
      configurationForMenuAtLocation: CGPoint(x: 100, y: 30)))
    XCTAssertEqual(presentations, 0)
    interaction.delegate?.contextMenuInteraction?(interaction, willDisplayMenuFor: configuration, animator: Animator())
    XCTAssertEqual(presentations, 1)
    view.leadingSwipeActions = self.configuration("Archive") { _, _ in }
    view.shadowListKit_swipeDistance(100, velocity: 0)
    XCTAssertNil(interaction.delegate?.contextMenuInteraction(interaction,
      configurationForMenuAtLocation: CGPoint(x: 100, y: 30)))
    view.contextMenu = UIMenu(children: [])
    XCTAssertFalse(view.interactions.contains { $0 is UIContextMenuInteraction })
    view.contextMenu = nil
    XCTAssertFalse(view.interactions.contains { $0 is UIContextMenuInteraction })
    XCTAssertEqual(presentations, 1)
  }
  func testAccessibleNativeChildOwnsStandaloneActionsWithoutAnExtraElement() throws {
    let view = ShadowListKitItemActionsView()
    let button = UIButton(type: .system)
    button.setTitle("Reservation", for: .normal)
    button.isAccessibilityElement = true
    let inspect = UIAccessibilityCustomAction(name: "Inspect") { _ in true }
    button.accessibilityCustomActions = [inspect]
    view.contentView.addSubview(button)
    var archived = 0
    view.leadingSwipeActions = configuration("Archive") { _, _ in archived += 1 }
    let window = attached(view)
    defer { window.isHidden = true }
    button.frame = view.contentView.bounds
    view.layoutIfNeeded()
    XCTAssertFalse(view.isAccessibilityElement)
    XCTAssertTrue(button.isAccessibilityElement)
    let actions = try XCTUnwrap(button.accessibilityCustomActions)
    XCTAssertEqual(actions.map(\.name), ["Inspect", "Archive"])
    XCTAssertTrue(actions[0] === inspect)
    let archive = try XCTUnwrap(actions.first { $0.name == "Archive" })
    XCTAssertEqual(archive.actionHandler?(archive), true)
    XCTAssertEqual(archived, 1)
    let share = UIAccessibilityCustomAction(name: "Share") { _ in true }
    button.accessibilityCustomActions = actions + [share]
    view.removeFromSuperview()
    XCTAssertEqual(button.accessibilityCustomActions?.map(\.name), ["Inspect", "Share"])
    XCTAssertEqual(archive.actionHandler?(archive), false)
    XCTAssertEqual(archived, 1)
    XCTAssertEqual(button.currentTitle, "Reservation")
    XCTAssertTrue(button.isAccessibilityElement)
  }

  func testHiddenAndExplicitAccessibilityTreesKeepTheirNativeBoundaries() throws {
    let view = ShadowListKitItemActionsView()
    let first = UIButton(type: .system)
    let second = UIButton(type: .system)
    first.setTitle("First", for: .normal); second.setTitle("Second", for: .normal)
    first.isAccessibilityElement = true; second.isAccessibilityElement = true
    let inspect = UIAccessibilityCustomAction(name: "Inspect") { _ in true }
    first.accessibilityCustomActions = [inspect]
    view.contentView.addSubview(first); view.contentView.addSubview(second)
    var invoked = 0
    view.leadingSwipeActions = configuration("Archive") { _, _ in invoked += 1 }
    let window = attached(view)
    defer { window.isHidden = true }
    let secondAction = try XCTUnwrap(second.accessibilityCustomActions?.first { $0.name == "Archive" })
    view.accessibilityElements = [first]
    XCTAssertEqual(secondAction.actionHandler?(secondAction), false)
    view.setNeedsLayout(); view.layoutIfNeeded()
    XCTAssertEqual(first.accessibilityCustomActions?.map(\.name), ["Inspect", "Archive"])
    XCTAssertTrue(second.accessibilityCustomActions?.isEmpty ?? true)
    let firstAction = try XCTUnwrap(first.accessibilityCustomActions?.first { $0.name == "Archive" })
    view.contentView.accessibilityElementsHidden = true
    XCTAssertEqual(firstAction.actionHandler?(firstAction), false)
    view.setNeedsLayout(); view.layoutIfNeeded()
    XCTAssertEqual(first.accessibilityCustomActions?.map(\.name), ["Inspect"])
    XCTAssertTrue(view.contentView.accessibilityElementsHidden)
    view.contentView.accessibilityElementsHidden = false
    view.setNeedsLayout(); view.layoutIfNeeded()
    XCTAssertEqual(first.accessibilityCustomActions?.map(\.name), ["Inspect", "Archive"])
    let restored = try XCTUnwrap(first.accessibilityCustomActions?.first { $0.name == "Archive" })
    view.accessibilityElements = []
    XCTAssertEqual(restored.actionHandler?(restored), false)
    view.setNeedsLayout(); view.layoutIfNeeded()
    XCTAssertEqual(first.accessibilityCustomActions?.map(\.name), ["Inspect"])
    XCTAssertEqual(view.accessibilityElements?.count, 0)
    XCTAssertEqual(invoked, 0)
  }

  func testGroupedRowDynamicProxyStaysCurrentWithoutDuplicateOwnedActions() throws {
    let row = ShadowListUtilsGroupedRow(title: "Reservation")
    let inspect = UIAccessibilityCustomAction(name: "Inspect") { _ in true }
    row.accessibilityCustomActions = [inspect]
    row.onLongPress = {}
    row.longPressLabel = "Details"
    row.leadingSwipeActions = configuration("Archive") { _, _ in }
    let window = attached(row)
    defer { window.isHidden = true }
    XCTAssertEqual(row.accessibilityCustomActions?.map(\.name), ["Inspect", "Details", "Archive"])
    row.leadingSwipeActions = configuration("Remove") { _, _ in }
    // Configuration forwarding alone updates the proxy; no unrelated row property is assigned.
    XCTAssertEqual(row.accessibilityCustomActions?.map(\.name), ["Inspect", "Details", "Remove"])
    for _ in 0..<3 { row.setNeedsLayout(); row.layoutIfNeeded() }
    XCTAssertEqual(row.accessibilityCustomActions?.map(\.name), ["Inspect", "Details", "Remove"])
    XCTAssertTrue(row.accessibilityCustomActions?.first === inspect)
    XCTAssertTrue(row.isAccessibilityElement)
  }

}
