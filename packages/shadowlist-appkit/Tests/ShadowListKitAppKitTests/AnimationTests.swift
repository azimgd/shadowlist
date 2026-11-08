import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * Change animations: the default animator's fades and slides, a custom animator, the batch
 * completion, deleted cells kept out of the pool while they fade, and animations next to a
 * drag, a pinned header and a driven offset.
 */
@MainActor
final class AnimationTests: XCTestCase {
  /*
   * Records the animator calls by key. A delete completes at once unless held.
   */
  private final class RecordingAnimator: NSObject, ShadowListKitItemAnimator {
    var inserted: [String] = []
    var deleted: [String] = []
    var moved: [String: CGPoint] = [:]
    var holdsDeletes = false
    var pending: [() -> Void] = []

    func listView(_ listView: ShadowListKitListView, animateInsertOf cell: ShadowListKitListCell) {
      inserted.append((cell as? TestCell)?.key ?? "")
    }

    func listView(_ listView: ShadowListKitListView, animateDeleteOf cell: ShadowListKitListCell, completion: @escaping () -> Void) {
      deleted.append((cell as? TestCell)?.key ?? "")
      if holdsDeletes {
        pending.append(completion)
      } else {
        completion()
      }
    }

    func listView(_ listView: ShadowListKitListView, animateMoveOf cell: ShadowListKitListCell, fromOffset offset: CGPoint) {
      moved[(cell as? TestCell)?.key ?? ""] = offset
    }
  }

  /*
   * Moves the keys of its source for a drop and applies a driven list's offsets.
   */
  private final class Host: NSObject, ShadowListKitListViewDelegate {
    let source: TestSource
    var corrections: [CGPoint] = []

    init(source: TestSource) {
      self.source = source
    }

    func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
      let key = source.keys.remove(at: sourceIndex)
      source.keys.insert(key, at: destinationIndex)
    }

    func listView(_ listView: ShadowListKitListView, didCorrectOffset contentOffset: CGPoint) {
      corrections.append(contentOffset)
      listView.contentOffset = contentOffset
    }
  }

  /*
   * Ten sections of ten 44 point items under a 30 point titled header.
   */
  private final class Sections: TestSource {
    var counts = Array(repeating: 10, count: 10)

    func numberOfSections(in listView: ShadowListKitListView) -> Int { counts.count }
    func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { counts[section] }
    func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { "Section \(section)" }
    func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat { 30 }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 44 }
  }

  private static let insertKey = "shadowListKitInsert"
  private static let deleteKey = "shadowListKitDelete"
  private static let moveKey = "shadowListKitMove"

  /*
   * A list that animates the changes after its first load, which would fade in every item.
   */
  private func makeHarness(count: Int = 200, configure: (ShadowListKitListView) -> Void = { _ in }) -> ListHarness {
    let harness = ListHarness(source: SizedTestSource(count: count), configure: configure)
    harness.list.animatesChanges = true
    return harness
  }

  private func cell(_ list: ShadowListKitListView, key: String, in source: TestSource) -> TestCell? {
    source.keys.firstIndex(of: key).flatMap { list.cellForItem(at: $0) as? TestCell }
  }

  private func keys(_ cell: NSView?) -> Set<String> {
    Set(cell?.layer?.animationKeys() ?? [])
  }

  /*
   * The offset a move animation starts from, or nil without one.
   */
  private func slide(_ cell: NSView?) -> CGPoint? {
    (cell?.layer?.animation(forKey: Self.moveKey) as? CABasicAnimation)?.fromValue.flatMap { ($0 as? NSValue)?.pointValue }
  }

  private func spin(until done: () -> Bool, timeout: TimeInterval = 2) {
    let end = Date().addingTimeInterval(timeout)
    while !done() && Date() < end {
      RunLoop.main.run(until: Date().addingTimeInterval(0.01))
    }
  }

  func testAnInsertFadesInTheNewItemAndSlidesTheItemsAfterIt() throws {
    let harness = makeHarness()
    let list = harness.list
    let source = harness.source
    source.keys.insert("new", at: 2)
    list.insertItems(at: IndexSet(integer: 2))
    harness.layout()
    let inserted = try XCTUnwrap(list.cellForItem(at: 2))
    XCTAssertTrue(keys(inserted).contains(Self.insertKey))
    XCTAssertTrue(keys(list.cellForItem(at: 0)).isEmpty)
    XCTAssertTrue(keys(list.cellForItem(at: 1)).isEmpty)
    // The items after it showed 44 points higher and slide down.
    let after = try XCTUnwrap(slide(list.cellForItem(at: 3)))
    XCTAssertEqual(after.x, 0, accuracy: 0.01)
    XCTAssertEqual(after.y, -44, accuracy: 0.01)
    // The slide adds to the frame. The drag's transform stays untouched.
    let layer = try XCTUnwrap(list.cellForItem(at: 3)?.layer)
    XCTAssertTrue((layer.animation(forKey: Self.moveKey) as? CABasicAnimation)?.isAdditive == true)
    XCTAssertNil(layer.animation(forKey: "transform"))
    XCTAssertTrue(CATransform3DIsIdentity(layer.transform))
  }

  /*
   * AppKit syncs a cell's layer on a frame change and when it builds the layer tree. A slide and
   * a transform the list set stay through both. A rotation of the cell still lands and comes off.
   */
  func testASlideAndATransformStayThroughARunLoopTurnAndAFrameChange() throws {
    let harness = makeHarness()
    let list = harness.list
    harness.source.keys.insert("new", at: 2)
    list.insertItems(at: IndexSet(integer: 2))
    harness.layout()
    let cell = try XCTUnwrap(list.cellForItem(at: 3))
    let layer = try XCTUnwrap(cell.layer)
    layer.transform = CATransform3DConcat(CATransform3DMakeScale(1.03, 1.03, 1), CATransform3DMakeTranslation(0, 44, 0))
    RunLoop.main.run(until: Date().addingTimeInterval(0.02))
    cell.frame = cell.frame.offsetBy(dx: 0, dy: 1)
    XCTAssertTrue((layer.animation(forKey: Self.moveKey) as? CABasicAnimation)?.isAdditive == true)
    XCTAssertEqual(layer.transform.m42, 44, accuracy: 0.01)
    XCTAssertEqual(layer.transform.m11, 1.03, accuracy: 0.0001)
    cell.frameCenterRotation = 10
    XCTAssertNotEqual(layer.transform.m12, 0)
    cell.frameCenterRotation = 0
    XCTAssertEqual(layer.transform.m11, 1, accuracy: 1e-9)
    XCTAssertEqual(layer.transform.m12, 0, accuracy: 1e-9)
    XCTAssertEqual(layer.transform.m42, 0, accuracy: 1e-9)
  }

  func testAChangeWhileTheListHasNoSizeDoesNotAnimateLater() throws {
    let harness = makeHarness()
    let list = harness.list
    let source = harness.source
    let frame = list.frame
    list.frame = NSRect(origin: frame.origin, size: .zero)
    harness.layout()
    source.keys.insert("new", at: 0)
    list.insertItems(at: IndexSet(integer: 0))
    harness.layout()
    list.frame = frame
    harness.layout()
    XCTAssertTrue(list.visibleCells.allSatisfy { keys($0).isEmpty })
    // The next change slides from where the items show now.
    source.keys.insert("next", at: 2)
    list.insertItems(at: IndexSet(integer: 2))
    harness.layout()
    let after = try XCTUnwrap(slide(list.cellForItem(at: 3)))
    XCTAssertEqual(after.y, -44, accuracy: 0.01)
  }

  func testTheSlideStartsWhereTheItemShowedBefore() throws {
    let harness = makeHarness()
    let list = harness.list
    let source = harness.source
    let moving = try XCTUnwrap(list.cellForItem(at: 4))
    let before = list.convert(moving.frame, from: moving.superview).minY
    source.keys.insert("new", at: 2)
    list.insertItems(at: IndexSet(integer: 2))
    harness.layout()
    CATransaction.flush()
    let layer = try XCTUnwrap(moving.layer)
    let presented = try XCTUnwrap(layer.presentation())
    // Right after it starts the cell still shows where it was.
    let shown = list.convert(moving.frame, from: moving.superview).minY + (presented.position.y - layer.position.y)
    XCTAssertEqual(shown, before, accuracy: 2)
  }

  func testADeleteFadesTheCellOutWhereItWasAndKeepsItOutOfThePool() throws {
    let harness = makeHarness()
    let list = harness.list
    let source = harness.source
    let fading = try XCTUnwrap(list.cellForItem(at: 2) as? TestCell)
    let frame = list.convert(fading.frame, from: fading.superview)
    source.keys.remove(at: 2)
    list.deleteItems(at: IndexSet(integer: 2))
    harness.layout()
    XCTAssertFalse(fading.isHidden)
    XCTAssertTrue(keys(fading).contains(Self.deleteKey))
    XCTAssertEqual(list.convert(fading.frame, from: fading.superview).minY, frame.minY, accuracy: 0.01)
    XCTAssertFalse(list.visibleCells.contains(fading))
    let slid = try XCTUnwrap(slide(list.cellForItem(at: 2)))
    XCTAssertEqual(slid.y, 44, accuracy: 0.01)
    // A new item does not get the fading cell.
    source.keys.insert("new", at: 0)
    list.insertItems(at: IndexSet(integer: 0))
    harness.layout()
    XCTAssertFalse(list.visibleCells.contains(fading))
    XCTAssertNotEqual(list.dequeueReusableCell(withIdentifier: "cell"), fading)
    // Once the fade ends it goes back to the pool.
    spin { fading.isHidden }
    XCTAssertTrue(fading.isHidden)
    XCTAssertEqual(fading.alphaValue, 1)
    XCTAssertTrue(keys(fading).isEmpty)
  }

  func testAMoveSlidesTheMovedItemAndTheItemsBetween() throws {
    let harness = makeHarness()
    let list = harness.list
    let source = harness.source
    let key = source.keys[1]
    source.keys.insert(source.keys.remove(at: 1), at: 4)
    list.moveItem(at: 1, to: 4)
    harness.layout()
    let moved = try XCTUnwrap(slide(cell(list, key: key, in: source)))
    XCTAssertEqual(moved.y, -132, accuracy: 0.01)
    for index in 1...3 {
      XCTAssertEqual(try XCTUnwrap(slide(list.cellForItem(at: index))).y, 44, accuracy: 0.01)
    }
    XCTAssertTrue(keys(list.cellForItem(at: 0)).isEmpty)
    XCTAssertTrue(keys(list.cellForItem(at: 5)).isEmpty)
  }

  func testNoAnimationRunsWithoutAnimatesChanges() throws {
    let animator = RecordingAnimator()
    let harness = ListHarness(source: SizedTestSource(count: 200)) { $0.itemAnimator = animator }
    let list = harness.list
    let source = harness.source
    XCTAssertFalse(list.animatesChanges)
    let deleted = try XCTUnwrap(list.cellForItem(at: 3))
    source.keys.insert("new", at: 1)
    source.keys.remove(at: 4)
    list.reloadData()
    harness.layout()
    XCTAssertTrue(deleted.isHidden)
    XCTAssertTrue(list.visibleCells.allSatisfy { keys($0).isEmpty })
    XCTAssertTrue(animator.inserted.isEmpty && animator.deleted.isEmpty && animator.moved.isEmpty)
    // Turned off, the default animator adds nothing either.
    list.itemAnimator = nil
    XCTAssertTrue(list.itemAnimator is ShadowListKitDefaultItemAnimator)
    list.animatesChanges = true
    list.animatesChanges = false
    source.keys.remove(at: 0)
    list.deleteItems(at: IndexSet(integer: 0))
    harness.layout()
    XCTAssertTrue(list.visibleCells.allSatisfy { keys($0).isEmpty })
  }

  func testACustomAnimatorGetsEveryChange() throws {
    let animator = RecordingAnimator()
    let harness = makeHarness { $0.itemAnimator = animator }
    let list = harness.list
    let source = harness.source
    let deletedKey = source.keys[3]
    let deleted = try XCTUnwrap(list.cellForItem(at: 3))
    source.keys.remove(at: 3)
    source.keys.insert("new", at: 1)
    list.reloadData()
    harness.layout()
    XCTAssertEqual(animator.inserted, ["new"])
    XCTAssertEqual(animator.deleted, [deletedKey])
    XCTAssertEqual(try XCTUnwrap(animator.moved[source.keys[2]]).y, -44, accuracy: 0.01)
    XCTAssertEqual(try XCTUnwrap(animator.moved[source.keys[4]]).y, 0, accuracy: 0.01)
    // The completion gave the cell back.
    XCTAssertTrue(deleted.isHidden)
    // A held completion keeps it shown until called.
    animator.holdsDeletes = true
    let held = try XCTUnwrap(list.cellForItem(at: 0))
    source.keys.remove(at: 0)
    list.deleteItems(at: IndexSet(integer: 0))
    harness.layout()
    XCTAssertFalse(held.isHidden)
    animator.pending.forEach { $0() }
    XCTAssertTrue(held.isHidden)
  }

  func testTheBatchCompletionFollowsTheAnimation() throws {
    let harness = makeHarness()
    let list = harness.list
    let source = harness.source
    let duration = try XCTUnwrap(list.itemAnimator as? ShadowListKitDefaultItemAnimator).duration
    let fading = try XCTUnwrap(list.cellForItem(at: 2))
    let start = CACurrentMediaTime()
    var finishedAt: CFTimeInterval?
    list.performBatchUpdates({
      source.keys.remove(at: 2)
      list.deleteItems(at: IndexSet(integer: 2))
      source.keys.insert("new", at: 4)
      list.insertItems(at: IndexSet(integer: 4))
    }) { finished in
      XCTAssertTrue(finished)
      finishedAt = CACurrentMediaTime()
    }
    // The batch is laid out and animating before it returns.
    XCTAssertTrue(keys(fading).contains(Self.deleteKey))
    XCTAssertTrue(keys(list.cellForItem(at: 4)).contains(Self.insertKey))
    spin { finishedAt != nil }
    let elapsed = try XCTUnwrap(finishedAt) - start
    XCTAssertGreaterThanOrEqual(elapsed, duration - 0.01)
    spin { fading.isHidden }
    XCTAssertTrue(fading.isHidden)

    // A custom animator's animations are not waited for.
    list.itemAnimator = RecordingAnimator()
    finishedAt = nil
    let customStart = CACurrentMediaTime()
    list.performBatchUpdates({
      source.keys.remove(at: 0)
      list.deleteItems(at: IndexSet(integer: 0))
    }) { _ in finishedAt = CACurrentMediaTime() }
    spin { finishedAt != nil }
    XCTAssertLessThan(try XCTUnwrap(finishedAt) - customStart, duration)
  }

  func testADropAnimatesItselfAndAHeldRowStopsChangeAnimations() throws {
    let source = SizedTestSource(count: 200)
    let harness = ListHarness(source: source) { $0.reorderEnabled = true }
    let list = harness.list
    list.animatesChanges = true
    let host = Host(source: source)
    list.delegate = host
    func center(_ index: Int) throws -> CGPoint {
      let cell = try XCTUnwrap(list.cellForItem(at: index))
      let frame = list.convert(cell.frame, from: cell.superview)
      return CGPoint(x: frame.midX, y: frame.midY)
    }
    list.shadowListKit_beginDrag(at: try center(1))
    list.shadowListKit_moveDrag(to: try center(4))
    // A change while a row is held is not animated.
    source.keys.append("tail")
    list.insertItems(at: IndexSet(integer: source.keys.count - 1))
    harness.layout()
    XCTAssertTrue(list.visibleCells.allSatisfy { keys($0).isDisjoint(with: [Self.insertKey, Self.moveKey]) })
    list.shadowListKit_endDrag()
    harness.layout()
    // The drop moves the data and slides the rows on their transform, with no change animation.
    XCTAssertEqual(source.keys[3], "item-1")
    XCTAssertTrue(list.visibleCells.allSatisfy { keys($0).isDisjoint(with: [Self.insertKey, Self.moveKey, Self.deleteKey]) })
    // The next change animates again on the position and leaves a drag shift alone.
    try XCTUnwrap(list.cellForItem(at: 5)?.layer).transform = CATransform3DMakeTranslation(0, 7, 0)
    source.keys.insert("new", at: 2)
    list.insertItems(at: IndexSet(integer: 2))
    harness.layout()
    XCTAssertTrue(keys(list.cellForItem(at: 2)).contains(Self.insertKey))
    let slid = try XCTUnwrap(list.cellForItem(at: 6)?.layer?.animation(forKey: Self.moveKey) as? CABasicAnimation)
    XCTAssertEqual(slid.keyPath, "position")
    XCTAssertEqual(try XCTUnwrap((slid.fromValue as? NSValue)?.pointValue).y, -44 + 7, accuracy: 0.01)
    XCTAssertNil(list.cellForItem(at: 6)?.layer?.animation(forKey: "transform"))
  }

  func testAPinnedHeaderStaysPinnedThroughAnAnimatedInsert() throws {
    let source = Sections(count: 100)
    let harness = ListHarness(source: source) { $0.stickySectionHeaders = true }
    let list = harness.list
    list.animatesChanges = true
    list.contentOffset = CGPoint(x: 0, y: 150)
    harness.layout()
    func pinned() -> ShadowListKitListCell? {
      list.subviews.first!.subviews.compactMap { $0 as? ShadowListKitListCell }.first { !$0.isHidden && $0.layer?.zPosition == 1 }
    }
    let header = try XCTUnwrap(pinned())
    let below = try XCTUnwrap(list.cellForItem(at: 6))
    let belowBefore = list.convert(below.frame, from: below.superview).minY
    // An item inserted in the shown part of section 0.
    source.keys.insert("new", at: 5)
    source.counts[0] += 1
    list.reloadData()
    harness.layout()
    XCTAssertEqual(pinned(), header)
    XCTAssertEqual(list.convert(header.frame, from: header.superview).minY, 0, accuracy: 0.01)
    XCTAssertNil(slide(header))
    XCTAssertTrue(keys(list.cellForItem(at: 5)).contains(Self.insertKey))
    let belowAfter = list.convert(below.frame, from: below.superview).minY
    XCTAssertEqual(try XCTUnwrap(slide(below)).y, belowBefore - belowAfter, accuracy: 0.01)
    // The pinned header follows the content while the rows slide under it.
    list.contentOffset = CGPoint(x: 0, y: 200)
    harness.layout()
    XCTAssertEqual(list.convert(header.frame, from: header.superview).minY, 0, accuracy: 0.01)
  }

  func testADrivenListAnimatesAndReportsItsCorrection() throws {
    let source = SizedTestSource(count: 600)
    let harness = ListHarness(source: source) {
      $0.isScrollDriven = true
      $0.allowsSelection = false
      $0.estimatedItemSize = 30
    }
    let list = harness.list
    list.animatesChanges = true
    let host = Host(source: source)
    list.delegate = host
    list.scrollPhase = .dragging
    list.contentOffset = CGPoint(x: 0, y: 3000)
    harness.layout()
    let before = harness.visibleKeys()
    let first = list.visibleRange.location
    source.keys.insert(contentsOf: (0..<10).map { "above-\($0)" }, at: 0)
    source.keys.remove(at: first + 13)
    list.reloadData()
    harness.layout()
    XCTAssertFalse(host.corrections.isEmpty)
    // The rows above the deleted one held still, the ones after it slide up.
    for index in (first + 10)..<(first + 12) {
      XCTAssertNil(slide(list.cellForItem(at: index)))
    }
    XCTAssertEqual(try XCTUnwrap(slide(list.cellForItem(at: first + 13))).y, 44, accuracy: 0.01)
    let unmoved = before.filter { key, _ in source.keys.firstIndex(of: key).map { $0 < first + 13 } ?? false }
    XCTAssertLessThan(harness.maxShift(from: unmoved).shift, 0.5)
  }
}
