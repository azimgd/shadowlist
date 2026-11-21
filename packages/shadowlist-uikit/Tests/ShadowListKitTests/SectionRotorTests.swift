import ShadowListKit
import UIKit
import XCTest

@MainActor
final class SectionRotorTests: XCTestCase {
  private struct Section {
    let key: String
    var count = 12
    var header = true
  }

  private final class HeaderCell: ShadowListKitListCell {
    let label = UILabel()
    override init(reuseIdentifier: String?) {
      super.init(reuseIdentifier: reuseIdentifier)
      label.isAccessibilityElement = true
      label.accessibilityTraits = .header
      addSubview(label)
    }
    override func layoutSubviews() { super.layoutSubviews(); label.frame = bounds }
  }

  private class Source: NSObject, ShadowListKitListViewDataSource {
    var sections = [Section(key: "A"), Section(key: "B"), Section(key: "C")]
    var keys: [String] { sections.flatMap { section in (0..<section.count).map { "\(section.key)-\($0)" } } }
    func numberOfItems(in listView: ShadowListKitListView) -> Int { keys.count }
    func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { keys[index] }
    func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
      listView.dequeueReusableCell(withIdentifier: "row")
    }
    func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 40 }
    func numberOfSections(in listView: ShadowListKitListView) -> Int { sections.count }
    func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { sections[section].count }
    func listView(_ listView: ShadowListKitListView, keyForSection section: Int) -> String { sections[section].key }
    func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? {
      sections[section].header ? "Header \(sections[section].key)" : nil
    }
    func listView(_ listView: ShadowListKitListView, cellForHeaderInSection section: Int) -> ShadowListKitListCell {
      let cell = listView.dequeueReusableCell(withIdentifier: "header") as! HeaderCell
      cell.label.text = "Header \(sections[section].key)"
      // Logic-host tests do not activate VoiceOver's lazy UILabel accessibility synthesis.
      cell.label.accessibilityLabel = cell.label.text
      return cell
    }
    func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat { 40 }
  }

  private final class UnlabelledFirstHeader: Source {
    override func listView(_ listView: ShadowListKitListView, cellForHeaderInSection section: Int) -> ShadowListKitListCell {
      let cell = listView.dequeueReusableCell(withIdentifier: "row")
      cell.isAccessibilityElement = section > 0
      cell.accessibilityLabel = section > 0 ? "Custom \(sections[section].key)" : nil
      return cell
    }
  }

  private func fixture(_ source: Source) -> (ShadowListKitListView, UIWindow) {
    let list = ShadowListKitListView()
    list.contentInsetAdjustmentBehavior = .never
    list.register(ShadowListKitListCell.self, forCellReuseIdentifier: "row")
    list.register(HeaderCell.self, forCellReuseIdentifier: "header")
    list.dataSource = source
    list.reloadData()
    return (list, host(list, size: CGSize(width: 200, height: 100)))
  }

  private func rotor(_ list: ShadowListKitListView) throws -> UIAccessibilityCustomRotor {
    try XCTUnwrap(list.accessibilityCustomRotors?.first { $0.systemRotorType == .heading })
  }

  private func search(_ rotor: UIAccessibilityCustomRotor, _ direction: UIAccessibilityCustomRotor.Direction,
                      from current: UIAccessibilityCustomRotorItemResult? = nil) -> UIAccessibilityCustomRotorItemResult? {
    let predicate = UIAccessibilityCustomRotorSearchPredicate()
    if let current { predicate.currentItem = current }
    predicate.searchDirection = direction
    return rotor.itemSearchBlock(predicate)
  }

  private func label(_ result: UIAccessibilityCustomRotorItemResult?) -> String? {
    (result?.targetElement as? NSObject)?.accessibilityLabel
  }

  func testForwardAndBackwardResolveRealVisibleAndOffscreenHeaders() throws {
    let source = Source()
    let (list, window) = fixture(source)
    let heading = try rotor(list)
    let first = search(heading, .next)
    XCTAssertEqual(label(first), "Header A")
    let second = search(heading, .next, from: first)
    XCTAssertEqual(label(second), "Header B")
    XCTAssertGreaterThan(list.contentOffset.y, 0)
    XCTAssertNotNil((second?.targetElement as? UIView)?.window)
    let previous = search(heading, .previous, from: second)
    XCTAssertEqual(label(previous), "Header A")
    XCTAssertNil(search(heading, .previous, from: previous))
    XCTAssertEqual(label(search(heading, .previous)), "Header C")
    withExtendedLifetime((source, window)) {}
  }

  func testEmptySectionsRemainHeadingsAndMissingHeadersAreSkipped() throws {
    let source = Source()
    source.sections = [Section(key: "A", count: 0), Section(key: "B", count: 3, header: false), Section(key: "C", count: 0)]
    let (list, window) = fixture(source)
    let heading = try rotor(list)
    let first = search(heading, .next)
    let second = search(heading, .next, from: first)
    XCTAssertEqual(label(first), "Header A")
    XCTAssertEqual(label(second), "Header C")
    XCTAssertNil(search(heading, .next, from: second))
    source.sections = []
    list.reloadData(); list.layoutIfNeeded()
    XCTAssertNil(search(heading, .next))
    XCTAssertNil(list.accessibilityCustomRotors)
    withExtendedLifetime((source, window)) {}
  }

  func testUnlabelledHeaderDoesNotBlockTheNextLabelledHeader() throws {
    let source = UnlabelledFirstHeader()
    source.sections = [Section(key: "A", count: 0), Section(key: "B", count: 0)]
    let (list, window) = fixture(source)
    XCTAssertEqual(label(search(try rotor(list), .next)), "Custom B")
    withExtendedLifetime((source, window)) {}
  }

  func testReloadReadsCurrentSectionKeysAndOrder() throws {
    let source = Source()
    let (list, window) = fixture(source)
    let heading = try rotor(list)
    source.sections = [Section(key: "C"), Section(key: "A")]
    list.reloadData(); list.layoutIfNeeded()
    let first = search(heading, .next)
    let second = search(heading, .next, from: first)
    XCTAssertEqual(label(first), "Header C")
    XCTAssertEqual(label(second), "Header A")
    XCTAssertNil(search(heading, .next, from: second))
    XCTAssertEqual(label(search(heading, .previous, from: second)), "Header C")
    withExtendedLifetime((source, window)) {}
  }

  func testOverlayContainerChildIsTheTargetAndResolvesCurrentSection() throws {
    let source = Source()
    let (list, window) = fixture(source)
    list.stickySectionHeaders = true
    var buttons: [String: UIButton] = [:]
    list.renderStickyHeaderOverlay = { key, _, _ in
      let container = UIView(frame: CGRect(x: 0, y: 0, width: 200, height: 40))
      let button = UIButton(frame: container.bounds)
      button.isAccessibilityElement = true
      button.accessibilityLabel = "Overlay \(key ?? "")"
      container.addSubview(button)
      container.accessibilityElements = [button]
      buttons[key ?? ""] = button
      return container
    }
    list.setNeedsLayout(); list.layoutIfNeeded()
    let heading = try rotor(list)
    let first = search(heading, .next)
    XCTAssertTrue(first?.targetElement as? UIButton === buttons["A"])
    XCTAssertEqual(label(first), "Overlay A")
    let second = search(heading, .next, from: first)
    // The existing focus scroll reveals B at the bottom; A remains the pinned overlay.
    XCTAssertEqual(label(second), "Header B")
    XCTAssertEqual(label(search(heading, .previous, from: second)), "Overlay A")
    withExtendedLifetime((source, window)) {}
  }

  func testDeletedOffscreenCurrentKeyCannotTargetTheRemovedSection() throws {
    let source = Source()
    let (list, window) = fixture(source)
    let heading = try rotor(list)
    // A's header and twelve items precede the unmounted B header.
    let oldHeader = try XCTUnwrap(list.accessibilityElement(at: 13) as? NSObject)
    let current = UIAccessibilityCustomRotorItemResult(targetElement: oldHeader, targetRange: nil)
    source.sections.remove(at: 1)
    list.reloadData(); list.layoutIfNeeded()
    XCTAssertEqual(label(search(heading, .next, from: current)), "Header A")
    withExtendedLifetime((source, window)) {}
  }

  func testCallerRotorsAndOrdinaryItemNavigationAreUnchanged() throws {
    let source = Source()
    let (list, window) = fixture(source)
    let cell = try XCTUnwrap(list.cellForItem(at: 0))
    let before = list.index(ofAccessibilityElement: cell)
    let offset = list.contentOffset
    _ = try rotor(list)
    XCTAssertEqual(list.index(ofAccessibilityElement: cell), before)
    XCTAssertEqual(list.contentOffset, offset)
    let custom = UIAccessibilityCustomRotor(name: "Caller") { _ in nil }
    list.accessibilityCustomRotors = [custom]
    XCTAssertTrue(list.accessibilityCustomRotors?.first === custom)
    list.accessibilityCustomRotors = []
    XCTAssertEqual(list.accessibilityCustomRotors?.count, 0)
    withExtendedLifetime((source, window)) {}
  }

  func testOrdinaryOffscreenItemFocusStillUsesTheKeyedScrollPath() async throws {
    let source = Source()
    let (list, window) = fixture(source)
    _ = try rotor(list)
    // The A header precedes item10, whose virtual element is at container index11.
    let element = try XCTUnwrap(list.accessibilityElement(at: 11) as? UIAccessibilityElement)
    let focused = expectation(description: "Existing accessibility focus scroll completed")
    element.accessibilityElementDidBecomeFocused()
    DispatchQueue.main.async {
      XCTAssertGreaterThan(list.contentOffset.y, 0)
      XCTAssertNotNil(list.cellForItem(at: 10))
      focused.fulfill()
    }
    await fulfillment(of: [focused], timeout: 2)
    withExtendedLifetime((source, window)) {}
  }

  func testDetachedRotorDoesNotScrollOrRetainTheList() throws {
    let source = Source()
    weak var weakList: ShadowListKitListView?
    var heading: UIAccessibilityCustomRotor?
    try autoreleasepool {
      let (list, window) = fixture(source)
      weakList = list
      heading = try rotor(list)
      list.removeFromSuperview()
      XCTAssertNil(search(heading!, .next))
      window.isHidden = true
    }
    XCTAssertNil(weakList)
    XCTAssertNil(search(try XCTUnwrap(heading), .next))
  }
}
