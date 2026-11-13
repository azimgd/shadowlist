import ShadowListKit
import ShadowListUtils
import UIKit

// MARK: - Sections

/*
 * Travellers grouped by the first letter of their name on the kit list itself: built-in section
 * headers that stick, a count in each section's footer, a section index, separators and Auto
 * Layout cells that size themselves. The data source gives no sizes.
 */
final class SectionsScreen: UIViewController, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  let list = ShadowListKitListView()
  private(set) var sections: [ShadowListUtilsContactSection] = []
  private var fresh = 5000

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Sections"
    view.backgroundColor = AppTheme.colors.background
    list.frame = view.bounds
    list.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    list.backgroundColor = AppTheme.colors.background
    list.estimatedItemSize = 64
    list.stickySectionHeaders = true
    list.renderStickyHeaderOverlay = { [weak self] _, _, section in
      guard let self, self.sections.indices.contains(section) else { return nil }
      let header = UIButton(type: .system)
      header.backgroundColor = AppTheme.colors.background
      header.contentHorizontalAlignment = .leading
      header.contentEdgeInsets = UIEdgeInsets(top: 12, left: 16, bottom: 12, right: 16)
      let title = "\(self.sections[section].title) · \(self.sections[section].items.count) travellers · Next section"
      header.setTitle(title, for: .normal)
      header.accessibilityLabel = title
      header.addAction(UIAction { [weak self] _ in
        guard let self, !self.sections.isEmpty else { return }
        self.list.scrollToSection((section + 1) % self.sections.count, animated: true)
      }, for: .touchUpInside)
      return header
    }
    list.showsSeparators = true
    list.separatorInsetStart = AppTheme.theme.rowInset
    list.register(AutoContactCell.self, forCellReuseIdentifier: "contact")
    list.dataSource = self
    list.delegate = self
    view.addSubview(list)
    group((0..<400).map { Items.contact(Contact(index: $0)) })
    list.reloadData()
    navigationItem.rightBarButtonItem = UIBarButtonItem(systemItem: .add, primaryAction: UIAction { [weak self] _ in self?.addTraveller() })
    FeatureScenario.startSections(self)
  }

  func group(_ contacts: [ShadowListUtilsContactItem]) {
    sections = shadowListUtilsGroupIntoSections(contacts.sorted { $0.name < $1.name })
  }

  var allContacts: [ShadowListUtilsContactItem] { sections.flatMap(\.items) }

  func contact(at index: Int) -> ShadowListUtilsContactItem { allContacts[index] }

  /*
   * A new traveller lands in its section. The list reads the sections again and keeps its place.
   */
  func addTraveller() {
    var contacts = allContacts
    contacts.append(Items.contact(Contact(index: fresh)))
    fresh += 1
    group(contacts)
    list.insertItems(at: IndexSet(integer: 0))
  }

  func numberOfItems(in listView: ShadowListKitListView) -> Int { allContacts.count }
  func numberOfSections(in listView: ShadowListKitListView) -> Int { sections.count }
  func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int { sections[section].items.count }
  func listView(_ listView: ShadowListKitListView, keyForSection section: Int) -> String { sections[section].key }
  func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? { sections[section].title }
  func listView(_ listView: ShadowListKitListView, titleForFooterInSection section: Int) -> String? {
    "\(sections[section].items.count) travellers"
  }
  func sectionIndexTitles(for listView: ShadowListKitListView) -> [String]? { sections.map(\.title) }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { contact(at: index).key }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "contact")
    let contact = contact(at: index)
    let subtitle = contact.subtitle ?? ""
    // Every fifth traveller has a longer note, which wraps and makes the row taller.
    (cell as? AutoContactCell)?.show(
      contact, note: index % 5 == 0 ? "\(subtitle) · Window seat, travelling with two bags and a guitar case that goes in the cabin." : subtitle)
    return cell
  }

  func listView(_ listView: ShadowListKitListView, didSelectItemAt index: Int) {
    listView.deselectItem(at: index, animated: true)
  }
}

/*
 * A row laid out with Auto Layout. ShadowListKitListCell fits its constraints for the row height.
 */
final class AutoContactCell: ShadowListKitListCell {
  private let avatar = ShadowListUtilsAvatar(theme: AppTheme.theme)
  private let name = UILabel()
  private let note = UILabel()

  override init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    backgroundColor = AppTheme.colors.background
    name.font = .preferredFont(forTextStyle: .body)
    name.textColor = AppTheme.colors.label
    note.font = .preferredFont(forTextStyle: .subheadline)
    note.textColor = AppTheme.colors.secondaryLabel
    note.numberOfLines = 0
    for view in [avatar, name, note] {
      view.translatesAutoresizingMaskIntoConstraints = false
      addSubview(view)
    }
    NSLayoutConstraint.activate([
      avatar.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 16),
      avatar.topAnchor.constraint(equalTo: topAnchor, constant: 12),
      avatar.widthAnchor.constraint(equalToConstant: avatar.size),
      avatar.heightAnchor.constraint(equalToConstant: avatar.size),
      avatar.bottomAnchor.constraint(lessThanOrEqualTo: bottomAnchor, constant: -12),
      name.leadingAnchor.constraint(equalTo: avatar.trailingAnchor, constant: 12),
      name.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -36),
      name.topAnchor.constraint(equalTo: topAnchor, constant: 11),
      note.leadingAnchor.constraint(equalTo: name.leadingAnchor),
      note.trailingAnchor.constraint(equalTo: name.trailingAnchor),
      note.topAnchor.constraint(equalTo: name.bottomAnchor, constant: 2),
      note.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -11),
    ])
  }

  func show(_ contact: ShadowListUtilsContactItem, note text: String) {
    avatar.show(name: contact.name, imageUrl: contact.avatarUrl, color: contact.avatarColor)
    name.text = contact.name
    note.text = text
  }

  override func setHighlighted(_ highlighted: Bool, animated: Bool) {
    super.setHighlighted(highlighted, animated: animated)
    UIView.animate(withDuration: animated ? 0.2 : 0) {
      self.backgroundColor = highlighted ? AppTheme.colors.elevated2 : AppTheme.colors.background
    }
  }
}

// MARK: - Inbox

/*
 * The library's inbox: swipe actions, a menu on a long press, selection while editing, pull to
 * refresh and separators. Changes go through setItems, which has the kit diff them by key and
 * animate them.
 */
final class InboxScreen: UIViewController {
  let list = ShadowListUtilsInboxList(theme: AppTheme.theme)
  private(set) var messages: [ShadowListUtilsInboxMessage] = (0..<200).map { Items.mail($0) }
  private var fresh = 1000
  private var savedPlace: ShadowListKitAnchorState?
  private(set) var refreshes = 0
  private var editButton: UIBarButtonItem?

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Inbox"
    view.backgroundColor = AppTheme.colors.background
    list.frame = view.bounds
    list.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    list.alwaysBounceVertical = true
    list.onPressItem = { [weak self] in self?.toggleRead($0) }
    list.onToggleRead = { [weak self] in self?.toggleRead($0) }
    list.onToggleFlag = { [weak self] in self?.toggleFlag($0) }
    list.onDelete = { [weak self] in self?.delete($0) }
    list.onReply = { [weak self] in self?.reply($0) }
    list.onSelectionChange = { [weak self] in self?.selectionChanged($0) }
    list.onRefresh = { [weak self] in self?.refresh() }
    view.addSubview(list)
    show()
    installActions()
    FeatureScenario.startInbox(self)
    // -SLSwipeDemo 1 opens the trailing actions of the second row, for screenshots.
    if UserDefaults.standard.string(forKey: "SLSwipeDemo") == "1" {
      DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
        self?.list.shadowListKit_swipeItem(at: 1, distance: -170, velocity: 0)
      }
    }
    // -SLSelectDemo 1 starts editing with the second and third rows selected, for screenshots.
    if UserDefaults.standard.string(forKey: "SLSelectDemo") == "1" {
      DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
        guard let self else { return }
        self.selectionChanged(Set(self.messages[1...2].map(\.key)))
      }
    }
  }

  private func installActions() {
    let edit = UIBarButtonItem(title: "Edit", primaryAction: UIAction { [weak self] _ in self?.toggleEditing() })
    let shuffle = UIAction(title: "Shuffle", image: UIImage(systemName: "shuffle")) { [weak self] _ in self?.shuffle() }
    let save = UIAction(title: "Save place", image: UIImage(systemName: "bookmark")) { [weak self] _ in self?.savedPlace = self?.list.anchorState }
    let restore = UIAction(title: "Back to saved place", image: UIImage(systemName: "bookmark.fill")) { [weak self] _ in
      if let place = self?.savedPlace { self?.list.restoreAnchorState(place) }
    }
    let more = UIBarButtonItem(image: UIImage(systemName: "ellipsis"), menu: UIMenu(children: [shuffle, save, restore]))
    editButton = edit
    navigationItem.rightBarButtonItems = [more, edit]
  }

  func show(animated: Bool = false) {
    list.setItems(messages, animated: animated)
  }

  func toggleEditing() {
    list.setEditing(!list.isEditing, animated: true)
    editButton?.title = list.isEditing ? "Done" : "Edit"
    title = "Inbox"
  }

  /*
   * A tap changed the selection while editing, or the menu's Select entry asked to start
   * editing with one message.
   */
  private func selectionChanged(_ keys: Set<String>) {
    if !list.isEditing {
      toggleEditing()
      list.selectedKeys = keys
    }
    title = keys.isEmpty ? "Inbox" : "\(keys.count) Selected"
  }

  /*
   * Shuffle a few messages, edit two and hand the list the new data to diff.
   */
  func shuffle() {
    let range = 2..<min(12, messages.count)
    let reversed = Array(messages[range].reversed())
    messages.replaceSubrange(range, with: reversed)
    messages[1].flagged.toggle()
    messages.insert(Items.mail(fresh), at: 4)
    fresh += 1
    messages.removeLast()
    show(animated: true)
  }

  func addNewMail(_ count: Int) {
    let new = (0..<count).map { offset -> ShadowListUtilsInboxMessage in
      var mail = Items.mail(fresh + offset)
      mail.read = false
      mail.receivedAtMs = Date().timeIntervalSince1970 * 1000
      return mail
    }
    fresh += count
    messages.insert(contentsOf: new, at: 0)
    show(animated: true)
  }

  private func change(_ message: ShadowListUtilsInboxMessage, _ edit: (inout ShadowListUtilsInboxMessage) -> Void) {
    guard let index = messages.firstIndex(where: { $0.key == message.key }) else { return }
    edit(&messages[index])
    show()
  }

  func toggleRead(_ message: ShadowListUtilsInboxMessage) {
    change(message) { $0.read.toggle() }
  }

  func toggleFlag(_ message: ShadowListUtilsInboxMessage) {
    change(message) { $0.flagged.toggle() }
  }

  func delete(_ message: ShadowListUtilsInboxMessage) {
    messages.removeAll { $0.key == message.key }
    show(animated: true)
  }

  private func reply(_ message: ShadowListUtilsInboxMessage) {
    let alert = UIAlertController(title: "Reply to \(message.sender)", message: message.subject, preferredStyle: .alert)
    alert.addAction(UIAlertAction(title: "OK", style: .default))
    present(alert, animated: true)
  }

  private func refresh() {
    refreshes += 1
    DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
      self?.addNewMail(3)
      self?.list.isRefreshing = false
    }
  }
}

// MARK: - Scenarios

/*
 * Scripted checks of the feature screens: -SLRoute Sections -SLScenario sections and
 * -SLRoute Inbox -SLScenario features. Each logs one [SLSCENARIO] JSON line.
 */
enum FeatureScenario {
  static func startSections(_ screen: SectionsScreen) {
    guard UserDefaults.standard.string(forKey: "SLScenario") == "sections" else { return }
    let list = screen.list
    after(1.5) {
      var result: [String: Any] = ["scenario": "sections"]
      result["numberOfSections"] = list.numberOfSections
      let third = list.firstItemIndexInSection(3)
      result["sectionOfThirdsFirstItem"] = list.sectionForItem(at: third)
      // Self-sizing: a row with the long note is taller than one without.
      let short = list.rectForItem(at: 1).height
      let tall = list.rectForItem(at: 0).height
      result["selfSizedShort"] = Double(short)
      result["selfSizedTall"] = Double(tall)
      list.scrollToSection(5, animated: false)
      list.layoutIfNeeded()
      let top = list.contentOffset.y + list.adjustedContentInset.top
      result["headerOffsetAfterScrollToSection"] = Double(list.rectForHeader(inSection: 5).minY - top)
      // Scroll into the section: its header stays pinned at the top.
      list.setContentOffset(CGPoint(x: 0, y: list.contentOffset.y + 120), animated: false)
      list.layoutIfNeeded()
      let viewportTop = list.contentOffset.y + list.adjustedContentInset.top
      let pinned = list.subviews.compactMap { $0 as? ShadowListKitListCell }.first { !$0.isHidden && $0.index == NSNotFound && $0.layer.zPosition == 1 }
      result["pinnedHeaderOffset"] = pinned.map { Double($0.frame.minY - viewportTop) } ?? NSNull()
      result["separators"] = list.visibleCells.filter { cell in cell.layer.sublayers?.contains { $0.zPosition == 1000 && !$0.isHidden } ?? false }.count
      // The section index is the list's one adjustable subview that is not a cell.
      result["hasSectionIndex"] = list.subviews.contains { !($0 is ShadowListKitListCell) && $0.accessibilityTraits.contains(.adjustable) }
      // Adding a traveller regroups the sections. Visible rows stay put.
      let before = visibleItems(list)
      screen.addTraveller()
      list.layoutIfNeeded()
      let afterAdd = visibleItems(list)
      var shift: CGFloat = 0
      for (key, y) in before { if let moved = afterAdd[key] { shift = max(shift, abs(moved - y)) } }
      result["maxShiftAfterRegroup"] = Double(shift)
      report(result)
    }
  }

  static func startInbox(_ screen: InboxScreen) {
    guard UserDefaults.standard.string(forKey: "SLScenario") == "features" else { return }
    let list = screen.list
    after(1.5) {
      var result: [String: Any] = ["scenario": "features"]
      let animator = CountingAnimator()
      list.itemAnimator = animator

      // Selection follows keys.
      screen.toggleEditing()
      list.selectedKeys = [screen.messages[3].key]
      screen.delete(screen.messages[0])
      list.layoutIfNeeded()
      result["selectedAfterDelete"] = Array(list.selectedIndices)
      result["selectedKeysAfterDelete"] = Array(list.selectedKeys)
      screen.toggleEditing()

      // setItems diffs the new data.
      screen.shuffle()
      list.layoutIfNeeded()
      result["shuffleConsistent"] = consistent(screen)

      // A changed item is configured again in the cell it has.
      let cellBefore = list.cellForItem(at: 2)
      screen.toggleRead(screen.messages[2])
      list.layoutIfNeeded()
      result["changeKeptCell"] = cellBefore != nil && cellBefore === list.cellForItem(at: 2)
      result["changeShown"] = ((cellBefore as? ShadowListUtilsRowView)?.row as? ShadowListUtilsInboxRow)?.item.read == screen.messages[2].read

      // Swipe the third row all the way: its delete runs.
      let count = screen.messages.count
      let swiped = screen.messages[2].key
      list.shadowListKit_swipeItem(at: 2, distance: -list.bounds.width, velocity: 0)
      after(0.6) {
        list.layoutIfNeeded()
        result["fullSwipeDeleted"] = screen.messages.count == count - 1 && !screen.messages.contains { $0.key == swiped }
        // A partial swipe opens the row. Closing it puts it back.
        list.shadowListKit_swipeItem(at: 1, distance: 120, velocity: 0)
        let open = list.cellForItem(at: 1)?.transform.tx ?? 0
        list.closeSwipeActions(animated: false)
        result["swipeOpenOffset"] = Double(open)
        result["swipeClosedOffset"] = Double(list.cellForItem(at: 1)?.transform.tx ?? -1)

        // The context menu has a configuration for a row.
        let interaction = list.interactions.compactMap { $0 as? UIContextMenuInteraction }.first
        let point = CGPoint(x: list.rectForItem(at: 4).midX, y: list.rectForItem(at: 4).midY)
        let configuration = interaction.flatMap { $0.delegate?.contextMenuInteraction($0, configurationForMenuAtLocation: point) }
        result["contextMenu"] = configuration != nil

        // Refresh through the control the list owns.
        list.refreshControl?.sendActions(for: .valueChanged)
        result["refreshCalls"] = screen.refreshes

        // The saved position lands again after rows above it changed.
        list.scrollToItem(at: 60, viewPosition: 0, animated: false)
        list.setContentOffset(CGPoint(x: 0, y: list.contentOffset.y + 13), animated: false)
        list.layoutIfNeeded()
        guard let place = list.anchorState else { return report(result) }
        screen.addNewMail(5)
        list.scrollToStart(animated: false)
        list.layoutIfNeeded()
        list.restoreAnchorState(place)
        list.layoutIfNeeded()
        let index = screen.messages.firstIndex { $0.key == place.key } ?? 0
        let viewportTop = list.contentOffset.y + list.adjustedContentInset.top
        result["anchorKey"] = place.key
        result["anchorError"] = Double((viewportTop - list.rectForItem(at: index).minY) - place.offset)
        result["animatorCalls"] = animator.calls
        result["separators"] = list.visibleCells.filter { cell in cell.layer.sublayers?.contains { $0.zPosition == 1000 && !$0.isHidden } ?? false }.count
        report(result)
      }
    }
  }

  /*
   * Whether every shown cell shows the message at its index.
   */
  private static func consistent(_ screen: InboxScreen) -> Bool {
    let range = screen.list.visibleRange
    guard range.location != NSNotFound, screen.list.rows.count == screen.messages.count else { return false }
    for index in range.location..<NSMaxRange(range) {
      guard let cell = screen.list.cellForItem(at: index) as? ShadowListUtilsRowView else { continue }
      if cell.row?.key != screen.messages[index].key { return false }
    }
    return true
  }

  private static func visibleItems(_ list: ShadowListKitListView) -> [String: CGFloat] {
    var rows: [String: CGFloat] = [:]
    for cell in list.visibleCells {
      if let key = list.dataSource?.listView(list, keyForItemAt: cell.index) {
        rows[key] = list.convert(cell.frame.origin, to: nil).y
      }
    }
    return rows
  }

  private static func report(_ result: [String: Any]) {
    let data = try! JSONSerialization.data(withJSONObject: result, options: .sortedKeys)
    print("[SLSCENARIO] \(String(data: data, encoding: .utf8)!)")
    fflush(stdout)
    if UserDefaults.standard.bool(forKey: "SLBenchExit") {
      exit(0)
    }
  }

  private static func after(_ seconds: Double, _ work: @escaping () -> Void) {
    DispatchQueue.main.asyncAfter(deadline: .now() + seconds, execute: work)
  }
}

/*
 * The default animations, counted.
 */
final class CountingAnimator: ShadowListKitDefaultItemAnimator {
  private(set) var calls = 0

  override func listView(_ listView: ShadowListKitListView, animateInsertOf cell: ShadowListKitListCell) {
    calls += 1
    super.listView(listView, animateInsertOf: cell)
  }

  override func listView(_ listView: ShadowListKitListView, animateDeleteOf cell: ShadowListKitListCell, completion: @escaping () -> Void) {
    calls += 1
    super.listView(listView, animateDeleteOf: cell, completion: completion)
  }

  override func listView(_ listView: ShadowListKitListView, animateMoveOf cell: ShadowListKitListCell, fromOffset offset: CGPoint) {
    calls += 1
    super.listView(listView, animateMoveOf: cell, fromOffset: offset)
  }
}
