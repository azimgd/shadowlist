import ShadowListKit
import UIKit

// MARK: - Sections

/*
 * Travellers grouped by the first letter of their name: built-in section headers that stick,
 * a count in each section's footer, a section index, separators and Auto Layout cells that size
 * themselves. The data source gives no sizes.
 */
final class SectionsScreen: UIViewController, SLKListViewDataSource, SLKListViewDelegate {
  let list = SLKListView()
  private(set) var sections: [(letter: String, contacts: [Contact])] = []
  private var fresh = 5000

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Sections"
    view.backgroundColor = Theme.background
    list.frame = view.bounds
    list.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    list.backgroundColor = Theme.background
    list.estimatedItemSize = 64
    list.stickySectionHeaders = true
    list.showsSeparators = true
    list.separatorInsetStart = 68
    list.register(AutoContactCell.self, forCellReuseIdentifier: "contact")
    list.dataSource = self
    list.delegate = self
    view.addSubview(list)
    group((0..<400).map { Contact(index: $0) })
    list.reloadData()
    navigationItem.rightBarButtonItem = UIBarButtonItem(systemItem: .add, primaryAction: UIAction { [weak self] _ in self?.addTraveller() })
    FeatureScenario.startSections(self)
  }

  func group(_ contacts: [Contact]) {
    let sorted = contacts.sorted { $0.author.name < $1.author.name }
    sections = Dictionary(grouping: sorted) { String($0.author.name.prefix(1)) }
      .map { (letter: $0.key, contacts: $0.value) }
      .sorted { $0.letter < $1.letter }
  }

  var allContacts: [Contact] { sections.flatMap(\.contacts) }

  func contact(at index: Int) -> Contact { allContacts[index] }

  /*
   * A new traveller lands in its section. The list reads the sections again and keeps its place.
   */
  func addTraveller() {
    var contacts = allContacts
    contacts.append(Contact(index: fresh))
    fresh += 1
    group(contacts)
    list.insertItems(at: IndexSet(integer: 0))
  }

  func numberOfItems(in listView: SLKListView) -> Int { allContacts.count }
  func numberOfSections(in listView: SLKListView) -> Int { sections.count }
  func listView(_ listView: SLKListView, numberOfItemsInSection section: Int) -> Int { sections[section].contacts.count }
  func listView(_ listView: SLKListView, keyForSection section: Int) -> String { sections[section].letter }
  func listView(_ listView: SLKListView, titleForHeaderInSection section: Int) -> String? { sections[section].letter }
  func listView(_ listView: SLKListView, titleForFooterInSection section: Int) -> String? {
    "\(sections[section].contacts.count) travellers"
  }
  func sectionIndexTitles(for listView: SLKListView) -> [String]? { sections.map(\.letter) }

  func listView(_ listView: SLKListView, keyForItemAt index: Int) -> String { contact(at: index).id }

  func listView(_ listView: SLKListView, cellForItemAt index: Int) -> SLKListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "contact") as! AutoContactCell
    let contact = contact(at: index)
    // Every fifth traveller has a longer note, which wraps and makes the row taller.
    cell.show(contact, note: index % 5 == 0 ? "\(contact.subtitle) · Window seat, travelling with two bags and a guitar case that goes in the cabin." : contact.subtitle)
    return cell
  }

  func listView(_ listView: SLKListView, didSelectItemAt index: Int) {
    listView.deselectItem(at: index, animated: true)
  }
}

/*
 * A row laid out with Auto Layout. SLKListCell fits its constraints for the row height.
 */
final class AutoContactCell: SLKListCell {
  private let avatar = UILabel()
  private let name = UILabel()
  private let note = UILabel()

  override init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    backgroundColor = Theme.background
    avatar.textAlignment = .center
    avatar.textColor = .white
    avatar.font = .systemFont(ofSize: 15, weight: .semibold)
    avatar.layer.cornerRadius = 20
    avatar.clipsToBounds = true
    name.font = .preferredFont(forTextStyle: .body)
    note.font = .preferredFont(forTextStyle: .subheadline)
    note.textColor = Theme.secondaryLabel
    note.numberOfLines = 0
    for view in [avatar, name, note] {
      view.translatesAutoresizingMaskIntoConstraints = false
      addSubview(view)
    }
    NSLayoutConstraint.activate([
      avatar.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 16),
      avatar.topAnchor.constraint(equalTo: topAnchor, constant: 12),
      avatar.widthAnchor.constraint(equalToConstant: 40),
      avatar.heightAnchor.constraint(equalToConstant: 40),
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

  required init?(coder: NSCoder) { fatalError() }

  func show(_ contact: Contact, note text: String) {
    avatar.text = contact.author.initials
    avatar.backgroundColor = contact.author.color
    name.text = contact.author.name
    note.text = text
  }

  override func setHighlighted(_ highlighted: Bool, animated: Bool) {
    super.setHighlighted(highlighted, animated: animated)
    UIView.animate(withDuration: animated ? 0.2 : 0) {
      self.backgroundColor = highlighted ? Theme.elevated2 : Theme.background
    }
  }
}

// MARK: - Inbox

/*
 * One message of the inbox.
 */
struct Message {
  let id: String
  let author: Author
  let text: String
  var read: Bool
  var flagged: Bool
  var version: Int

  init(index: Int) {
    id = "message-\(index)"
    author = Author(FixtureStrings.characterNames[index % FixtureStrings.characterNames.count])
    text = FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.count]
    read = index % 3 == 0
    flagged = false
    version = 0
  }
}

/*
 * An inbox that uses the list's interaction features: swipe actions, a context menu, selection
 * with editing, pull to refresh, separators, batch updates, applyChanges with content versions,
 * payload reloads, prefetching and a saved scroll position. Changes animate.
 */
final class InboxScreen: UIViewController, SLKListViewDataSource, SLKListViewDelegate, SLKListViewPrefetchDataSource {
  let list = SLKListView()
  var messages: [Message] = (0..<200).map { Message(index: $0) }
  private var fresh = 1000
  private var savedPlace: SLKAnchorState?
  private(set) var prefetched = 0
  private(set) var cancelled = 0
  private(set) var reconfigured = 0
  private(set) var refreshes = 0
  var onSelectionChange: (() -> Void)?

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Inbox"
    view.backgroundColor = Theme.background
    list.frame = view.bounds
    list.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    list.backgroundColor = Theme.background
    list.estimatedItemSize = 76
    list.animatesChanges = true
    list.showsSeparators = true
    list.separatorInsetStart = 72
    list.refreshEnabled = true
    list.reorderEnabled = Settings.debug
    list.register(MessageCell.self, forCellReuseIdentifier: "message")
    list.dataSource = self
    list.delegate = self
    list.prefetchDataSource = self
    view.addSubview(list)
    list.reloadData()
    installActions()
    FeatureScenario.startInbox(self)
    // -SLSwipeDemo 1 opens the trailing actions of the second row, for screenshots.
    if UserDefaults.standard.string(forKey: "SLSwipeDemo") == "1" {
      DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
        self?.list.slk_swipeItem(at: 1, distance: -170, velocity: 0)
      }
    }
  }

  private func installActions() {
    let edit = UIBarButtonItem(title: "Edit", primaryAction: UIAction { [weak self] _ in self?.toggleEditing() })
    let shuffle = UIAction(title: "Shuffle with applyChanges", image: UIImage(systemName: "shuffle")) { [weak self] _ in self?.shuffle() }
    let batch = UIAction(title: "Batch update", image: UIImage(systemName: "square.stack")) { [weak self] _ in self?.batchUpdate() }
    let save = UIAction(title: "Save place", image: UIImage(systemName: "bookmark")) { [weak self] _ in self?.savedPlace = self?.list.anchorState }
    let restore = UIAction(title: "Back to saved place", image: UIImage(systemName: "bookmark.fill")) { [weak self] _ in
      if let place = self?.savedPlace { self?.list.restoreAnchorState(place) }
    }
    let more = UIBarButtonItem(image: UIImage(systemName: "ellipsis"), menu: UIMenu(children: [shuffle, batch, save, restore]))
    navigationItem.rightBarButtonItems = [more, edit]
  }

  func toggleEditing() {
    list.setEditing(!list.isEditing, animated: true)
    list.allowsMultipleSelection = list.isEditing
    navigationItem.rightBarButtonItems?.last?.title = list.isEditing ? "Done" : "Edit"
  }

  /*
   * New mail on top, a removed message and a moved one, all in one animated pass.
   */
  func batchUpdate() {
    guard messages.count > 6 else { return }
    // Before: m0 m1 m2 m3 m4 m5. After: new m5 m1 m2 m3 m4, with m2 marked read or unread.
    let moved = messages.remove(at: 5)
    messages.remove(at: 0)
    messages.insert(moved, at: 0)
    messages.insert(Message(index: fresh), at: 0)
    fresh += 1
    messages[3].read.toggle()
    list.performBatchUpdates({
      list.deleteItems(at: IndexSet(integer: 0))
      list.moveItem(at: 5, to: 1)
      list.insertItems(at: IndexSet(integer: 0))
      list.reloadItems(at: IndexSet(integer: 2))
    }, completion: nil)
  }

  /*
   * Shuffle a few messages, edit two and hand the list the new data to diff.
   */
  @discardableResult
  func shuffle() -> SLKListChanges {
    let range = 2..<min(12, messages.count)
    let reversed = Array(messages[range].reversed())
    messages.replaceSubrange(range, with: reversed)
    messages[1].version += 1
    messages[1].flagged.toggle()
    messages.insert(Message(index: fresh), at: 4)
    fresh += 1
    messages.remove(at: messages.count - 1)
    return list.applyChanges()
  }

  func addNewMail(_ count: Int) {
    let new = (0..<count).map { offset in Message(index: fresh + offset) }
    fresh += count
    messages.insert(contentsOf: new, at: 0)
    list.insertItems(at: IndexSet(0..<count))
  }

  func numberOfItems(in listView: SLKListView) -> Int { messages.count }
  func listView(_ listView: SLKListView, keyForItemAt index: Int) -> String { messages[index].id }
  func listView(_ listView: SLKListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat { 76 }
  func listView(_ listView: SLKListView, contentVersionForItemAt index: Int) -> Int { messages[index].version }

  func listView(_ listView: SLKListView, cellForItemAt index: Int) -> SLKListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "message") as! MessageCell
    cell.show(messages[index])
    return cell
  }

  func listView(_ listView: SLKListView, reconfigureCell cell: SLKListCell, at index: Int, payload: Any?) -> Bool {
    guard let cell = cell as? MessageCell, payload as? String == "read" else { return false }
    reconfigured += 1
    cell.show(messages[index])
    return true
  }

  func listView(_ listView: SLKListView, prefetchItemsAt indices: IndexSet) { prefetched += indices.count }
  func listView(_ listView: SLKListView, cancelPrefetchingForItemsAt indices: IndexSet) { cancelled += indices.count }

  func listView(_ listView: SLKListView, didSelectItemAt index: Int) {
    if !listView.isEditing {
      listView.deselectItem(at: index, animated: true)
      toggleRead(index)
    }
    onSelectionChange?()
  }

  func listView(_ listView: SLKListView, didDeselectItemAt index: Int) { onSelectionChange?() }

  func toggleRead(_ index: Int) {
    messages[index].read.toggle()
    list.reloadItems(at: IndexSet(integer: index), payload: "read")
  }

  func delete(_ index: Int) {
    messages.remove(at: index)
    list.deleteItems(at: IndexSet(integer: index))
  }

  func listView(_ listView: SLKListView, leadingSwipeActionsForItemAt index: Int) -> SLKSwipeActionsConfiguration? {
    let read = SLKSwipeAction(style: .normal, title: messages[index].read ? "Unread" : "Read") { [weak self] _, done in
      self?.toggleRead(index)
      done(true)
    }
    read.backgroundColor = Theme.accent
    return SLKSwipeActionsConfiguration(actions: [read])
  }

  func listView(_ listView: SLKListView, trailingSwipeActionsForItemAt index: Int) -> SLKSwipeActionsConfiguration? {
    let key = messages[index].id
    let delete = SLKSwipeAction(style: .destructive, title: "Delete") { [weak self] _, done in
      guard let self, let at = self.messages.firstIndex(where: { $0.id == key }) else { return done(false) }
      self.delete(at)
      done(true)
    }
    let flag = SLKSwipeAction(style: .normal, title: "Flag") { [weak self] _, done in
      guard let self, let at = self.messages.firstIndex(where: { $0.id == key }) else { return done(false) }
      self.messages[at].flagged.toggle()
      self.list.reloadItems(at: IndexSet(integer: at), payload: "read")
      done(true)
    }
    flag.backgroundColor = .systemOrange
    return SLKSwipeActionsConfiguration(actions: [delete, flag])
  }

  func listView(_ listView: SLKListView, contextMenuForItemAt index: Int) -> UIMenu? {
    let key = messages[index].id
    let at = { [weak self] in self?.messages.firstIndex(where: { $0.id == key }) }
    return UIMenu(children: [
      UIAction(title: "Pin to Top", image: UIImage(systemName: "pin")) { [weak self] _ in
        guard let self, let from = at() else { return }
        let message = self.messages.remove(at: from)
        self.messages.insert(message, at: 0)
        self.list.moveItem(at: from, to: 0)
      },
      UIAction(title: messages[index].read ? "Mark Unread" : "Mark Read", image: UIImage(systemName: "envelope")) { [weak self] _ in
        if let from = at() { self?.toggleRead(from) }
      },
      UIAction(title: "Delete", image: UIImage(systemName: "trash"), attributes: .destructive) { [weak self] _ in
        if let from = at() { self?.delete(from) }
      },
    ])
  }

  func listViewDidBeginRefreshing(_ listView: SLKListView) {
    refreshes += 1
    DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
      self?.addNewMail(3)
      self?.list.isRefreshing = false
    }
  }
}

/*
 * A message row: avatar, sender, text, an unread dot and a flag. It shows highlight, selection
 * and, while editing, a check circle.
 */
final class MessageCell: SLKListCell {
  private let avatar = UILabel()
  private let sender = UILabel()
  private let body = UILabel()
  private let dot = UIView()
  private let flag = UIImageView(image: UIImage(systemName: "flag.fill"))
  private let check = UIImageView()

  override init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    backgroundColor = Theme.background
    avatar.textAlignment = .center
    avatar.textColor = .white
    avatar.font = .systemFont(ofSize: 15, weight: .semibold)
    avatar.layer.cornerRadius = 22
    avatar.clipsToBounds = true
    sender.font = .systemFont(ofSize: 16, weight: .semibold)
    body.font = .systemFont(ofSize: 14)
    body.textColor = Theme.secondaryLabel
    body.numberOfLines = 2
    dot.backgroundColor = Theme.accent
    dot.layer.cornerRadius = 5
    flag.tintColor = .systemOrange
    check.tintColor = Theme.accent
    [avatar, sender, body, dot, flag, check].forEach(addSubview)
  }

  required init?(coder: NSCoder) { fatalError() }

  func show(_ message: Message) {
    avatar.text = message.author.initials
    avatar.backgroundColor = message.author.color
    sender.text = message.author.name
    body.text = message.text
    dot.isHidden = message.read
    flag.isHidden = !message.flagged
    accessibilityLabel = "\(message.author.name), \(message.read ? "read" : "unread"), \(message.text)"
    isAccessibilityElement = true
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    let width = bounds.width
    let lead: CGFloat = isEditing ? 40 : 0
    check.frame = CGRect(x: 12, y: 26, width: 24, height: 24)
    check.alpha = isEditing ? 1 : 0
    dot.frame = CGRect(x: lead + 4, y: 33, width: 10, height: 10)
    avatar.frame = CGRect(x: lead + 18, y: 16, width: 44, height: 44)
    sender.frame = CGRect(x: lead + 72, y: 10, width: width - lead - 72 - 40, height: 20)
    body.frame = CGRect(x: lead + 72, y: 31, width: width - lead - 72 - 16, height: 36)
    flag.frame = CGRect(x: width - 34, y: 10, width: 18, height: 18)
  }

  override func setHighlighted(_ highlighted: Bool, animated: Bool) {
    super.setHighlighted(highlighted, animated: animated)
    updateBackground(animated)
  }

  override func setSelected(_ selected: Bool, animated: Bool) {
    super.setSelected(selected, animated: animated)
    check.image = UIImage(systemName: selected ? "checkmark.circle.fill" : "circle")
    updateBackground(animated)
  }

  override func setEditing(_ editing: Bool, animated: Bool) {
    super.setEditing(editing, animated: animated)
    check.image = UIImage(systemName: isSelected ? "checkmark.circle.fill" : "circle")
    UIView.animate(withDuration: animated ? 0.25 : 0) { self.layoutSubviews() }
  }

  private func updateBackground(_ animated: Bool) {
    UIView.animate(withDuration: animated ? 0.2 : 0) {
      self.backgroundColor = self.isHighlighted ? Theme.elevated2 : self.isSelected ? Theme.elevated : Theme.background
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
      let pinned = list.subviews.compactMap { $0 as? SLKListCell }.first { !$0.isHidden && $0.index == NSNotFound && $0.layer.zPosition == 1 }
      result["pinnedHeaderOffset"] = pinned.map { Double($0.frame.minY - viewportTop) } ?? NSNull()
      result["separators"] = list.visibleCells.filter { cell in cell.layer.sublayers?.contains { $0.zPosition == 1000 && !$0.isHidden } ?? false }.count
      result["hasSectionIndex"] = list.subviews.contains { String(describing: type(of: $0)) == "SLKSectionIndexView" }
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
      list.allowsMultipleSelection = true
      list.selectItem(at: 1, animated: false)
      list.selectItem(at: 3, animated: false)
      list.deselectItem(at: 1, animated: false)
      screen.messages.remove(at: 0)
      list.deleteItems(at: IndexSet(integer: 0))
      list.layoutIfNeeded()
      result["selectedAfterDelete"] = Array(list.selectedIndices)
      list.allowsMultipleSelection = false

      // A batch: new mail, a delete, a move and a reload.
      screen.batchUpdate()
      list.layoutIfNeeded()
      result["batchConsistent"] = consistent(screen)

      // applyChanges diffs the new data.
      let changes = screen.shuffle()
      list.layoutIfNeeded()
      result["applyChanges"] = ["deleted": changes.deletedIndices.count, "inserted": changes.insertedIndices.count,
                                "moved": changes.movedFromIndices.count, "reloaded": changes.reloadedIndices.count]
      result["applyConsistent"] = consistent(screen)

      // A payload reload updates the shown cell in place.
      let cellBefore = list.cellForItem(at: 2)
      screen.toggleRead(2)
      list.layoutIfNeeded()
      result["payloadKeptCell"] = cellBefore != nil && cellBefore === list.cellForItem(at: 2)
      result["reconfigured"] = screen.reconfigured

      // Swipe the third row all the way: its delete runs.
      let count = screen.messages.count
      let swiped = screen.messages[2].id
      list.slk_swipeItem(at: 2, distance: -list.bounds.width, velocity: 0)
      after(0.6) {
        list.layoutIfNeeded()
        result["fullSwipeDeleted"] = screen.messages.count == count - 1 && !screen.messages.contains { $0.id == swiped }
        // A partial swipe opens the row. Closing it puts it back.
        list.slk_swipeItem(at: 1, distance: 120, velocity: 0)
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
        result["refreshDelegateCalls"] = screen.refreshes

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
        let index = screen.messages.firstIndex { $0.id == place.key } ?? 0
        let viewportTop = list.contentOffset.y + list.adjustedContentInset.top
        result["anchorKey"] = place.key
        result["anchorError"] = Double((viewportTop - list.rectForItem(at: index).minY) - place.offset)

        // Prefetching followed the scrolling.
        result["prefetched"] = screen.prefetched
        result["cancelled"] = screen.cancelled
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
    guard range.location != NSNotFound else { return false }
    for index in range.location..<NSMaxRange(range) {
      guard let cell = screen.list.cellForItem(at: index) else { continue }
      if cell.accessibilityLabel?.hasPrefix(screen.messages[index].author.name) != true { return false }
    }
    return screen.list.numberOfItemsCheck(screen.messages.count)
  }

  private static func visibleItems(_ list: SLKListView) -> [String: CGFloat] {
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
final class CountingAnimator: SLKDefaultItemAnimator {
  private(set) var calls = 0

  override func listView(_ listView: SLKListView, animateInsertOf cell: SLKListCell) {
    calls += 1
    super.listView(listView, animateInsertOf: cell)
  }

  override func listView(_ listView: SLKListView, animateRemovalOf cell: SLKListCell, completion: @escaping () -> Void) {
    calls += 1
    super.listView(listView, animateRemovalOf: cell, completion: completion)
  }

  override func listView(_ listView: SLKListView, animateMoveOf cell: SLKListCell, fromOffset offset: CGPoint) {
    calls += 1
    super.listView(listView, animateMoveOf: cell, fromOffset: offset)
  }
}

private extension SLKListView {
  /*
   * Whether the list holds count items: the last one has a frame and the one after does not.
   */
  func numberOfItemsCheck(_ count: Int) -> Bool {
    !rectForItem(at: count - 1).isNull && rectForItem(at: count).isNull
  }
}
