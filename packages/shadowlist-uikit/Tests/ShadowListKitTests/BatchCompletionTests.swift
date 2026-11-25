import ShadowListKit
import UIKit
import XCTest

/*
 * performBatchUpdates:completion: calls completion once the change animation the batch started
 * ended, and right after the layout when nothing animates.
 */
@MainActor
final class BatchCompletionTests: XCTestCase {
  private final class Rows: NSObject, ShadowListKitListViewDataSource {
    var keys = (0 ..< 20).map { "item-\($0)" }
    func numberOfItems(in listView: ShadowListKitListView) -> Int {
      keys.count
    }

    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
      keys[index]
    }

    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "row")
    }

    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
      40
    }
  }

  private func spin(until done: () -> Bool) {
    let deadline = Date(timeIntervalSinceNow: 3)
    while !done(), Date() < deadline {
      RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.01))
    }
  }

  func testTheCompletionFollowsTheChangeAnimationAndComesAtOnceWithoutOne() throws {
    let rows = Rows()
    let list = ShadowListKitListView()
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.dataSource = rows
    list.reloadData()
    let window = host(list, size: CGSize(width: 300, height: 400))
    defer { window.isHidden = true }
    list.animatesChanges = true
    let duration = try XCTUnwrap(list.itemAnimator as? ShadowListKitDefaultItemAnimator).duration
    var finishedAt: CFTimeInterval?
    var calls = 0
    let start = CACurrentMediaTime()
    list.performBatchUpdates({
      rows.keys.remove(at: 2)
      list.deleteItems(at: IndexSet(integer: 2))
      rows.keys.insert("new", at: 4)
      list.insertItems(at: IndexSet(integer: 4))
    }) { finished in
      XCTAssertTrue(finished)
      calls += 1
      finishedAt = CACurrentMediaTime()
    }
    XCTAssertNil(finishedAt)
    spin { finishedAt != nil }
    XCTAssertGreaterThanOrEqual(try XCTUnwrap(finishedAt) - start, duration - 0.02)
    RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.05))
    XCTAssertEqual(calls, 1)

    // Without an animation the completion comes on the next turn of the run loop.
    list.animatesChanges = false
    finishedAt = nil
    let plainStart = CACurrentMediaTime()
    list.performBatchUpdates({
      rows.keys.remove(at: 0)
      list.deleteItems(at: IndexSet(integer: 0))
    }) { _ in finishedAt = CACurrentMediaTime() }
    XCTAssertNil(finishedAt)
    spin { finishedAt != nil }
    XCTAssertLessThan(try XCTUnwrap(finishedAt) - plainStart, duration)
  }

  func testANestedBatchWaitsForTheAnimationsStartedByTheOuterCommit() throws {
    for completesOuter in [false, true] {
      let rows = Rows()
      let list = ShadowListKitListView()
      list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
      list.dataSource = rows
      list.reloadData()
      let window = host(list, size: CGSize(width: 300, height: 400))
      defer { window.isHidden = true }
      list.animatesChanges = true
      let fading = try XCTUnwrap(list.cellForItem(at: 2))
      let duration = try XCTUnwrap(list.itemAnimator as? ShadowListKitDefaultItemAnimator).duration
      let start = CACurrentMediaTime()
      var calls = 0
      let completion: (Bool) -> Void = { finished in
        XCTAssertTrue(finished)
        XCTAssertTrue(fading.isHidden)
        XCTAssertGreaterThanOrEqual(CACurrentMediaTime() - start, duration - 0.02)
        calls += 1
      }
      list.performBatchUpdates({
        list.performBatchUpdates({
          rows.keys.remove(at: 2)
          list.deleteItems(at: IndexSet(integer: 2))
        }, completion: completion)
        rows.keys.insert("new", at: 4)
        list.insertItems(at: IndexSet(integer: 4))
      }, completion: completesOuter ? completion : nil)
      XCTAssertEqual(calls, 0)
      let expected = completesOuter ? 2 : 1
      spin { calls >= expected }
      XCTAssertEqual(calls, expected)
    }
  }

  func testUnrelatedAnimationsDoNotHoldTheBatchCompletion() throws {
    let rows = Rows()
    let list = ShadowListKitListView()
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.dataSource = rows
    list.reloadData()
    let window = host(list, size: CGSize(width: 300, height: 400))
    defer { window.isHidden = true }
    list.animatesChanges = true
    let before = try XCTUnwrap(list.cellForItem(at: 0)).layer
    let after = try XCTUnwrap(list.cellForItem(at: 1)).layer
    let unrelated = CABasicAnimation(keyPath: "opacity")
    unrelated.fromValue = 1
    unrelated.toValue = 0.5
    unrelated.duration = 1
    unrelated.repeatCount = .infinity
    before.add(unrelated, forKey: "unrelated")
    defer {
      before.removeAnimation(forKey: "unrelated")
      after.removeAnimation(forKey: "unrelated")
    }
    var calls = 0
    list.performBatchUpdates({
      rows.keys.remove(at: 2)
      list.deleteItems(at: IndexSet(integer: 2))
    }) { finished in
      XCTAssertTrue(finished)
      calls += 1
    }
    after.add(unrelated, forKey: "unrelated")
    XCTAssertEqual(calls, 0)
    spin { calls != 0 }
    XCTAssertEqual(calls, 1)
    XCTAssertNotNil(before.animation(forKey: "unrelated"))
    XCTAssertNotNil(after.animation(forKey: "unrelated"))
  }
}
