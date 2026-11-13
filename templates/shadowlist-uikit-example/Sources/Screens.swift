import ShadowListKit
import ShadowListUtils
import UIKit

/*
 * The fake network: replies come back on the main queue after SLLatency min,max milliseconds,
 * 250 to 700 by default like the React Native example.
 */
enum FakeNetwork {
  static let latency: ClosedRange<Double> = {
    let parts = (UserDefaults.standard.string(forKey: "SLLatency") ?? "250,700").split(separator: ",").compactMap { Double($0) }
    return parts.count == 2 ? min(parts[0], parts[1])...max(parts[0], parts[1]) : 250...700
  }()

  static func reply(_ work: @escaping () -> Void) {
    DispatchQueue.main.asyncAfter(deadline: .now() + Double.random(in: latency) / 1000, execute: work)
  }
}

/*
 * A screen with one list and the header actions every example has: add rows at the start,
 * add rows at the end and jump to a random row. With the sl engine the list is the screen's
 * ShadowListUtils template. The other engines host the same rows for comparison.
 */
class ListScreen: UIViewController {
  let engine = Settings.engine
  var list: ListController!

  /*
   * The template list the sl engine shows, the grid columns and whether the list is inverted.
   */
  func makeTemplate() -> ShadowListUtilsList? { nil }
  var columns: Int { 1 }
  var inverted: Bool { false }

  func prependRows() {}
  func appendRows() {}

  /*
   * Synchronous versions for the update cost scenario, without the fake network.
   */
  func prependRowsNow() { prependRows() }
  func appendRowsNow() { appendRows() }
  var prependTitle: String { "Prepend" }
  var appendTitle: String { "Append" }

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = AppTheme.colors.background
    list = ListController(engine: engine, template: makeTemplate(), columns: columns, inverted: inverted)
    list.view.translatesAutoresizingMaskIntoConstraints = false
    list.view.accessibilityIdentifier = "list"
    view.addSubview(list.view)
    layoutList()
    installActions()
    let opaque = UINavigationBarAppearance()
    opaque.configureWithOpaqueBackground()
    opaque.backgroundColor = AppTheme.colors.background
    navigationItem.standardAppearance = opaque
    navigationItem.scrollEdgeAppearance = opaque
    navigationItem.largeTitleDisplayMode = .never
    DispatchQueue.main.async { Scenario.start(self) }
  }

  func layoutList() {
    NSLayoutConstraint.activate([
      list.view.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
      list.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      list.view.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      list.view.bottomAnchor.constraint(equalTo: view.bottomAnchor),
    ])
  }

  /*
   * Show the screen's items: through the template's setter with the sl engine, and as rows the
   * controller hosts with the others.
   */
  func show<Item>(_ items: [Item], _ change: RowChange, template: ([Item]) -> Void, row: (Item) -> ShadowListUtilsRow) {
    if list.template != nil {
      list.update { template(items) }
    } else {
      list.setRows(items.map(row), change: change)
    }
  }

  private func installActions() {
    let prepend = UIAction(title: prependTitle, image: UIImage(systemName: "arrow.up.to.line")) { [weak self] _ in self?.prependRows() }
    let append = UIAction(title: appendTitle, image: UIImage(systemName: "arrow.down.to.line")) { [weak self] _ in self?.appendRows() }
    let random = UIAction(title: "Jump to Random Item", image: UIImage(systemName: "scope")) { [weak self] _ in
      guard let self, !self.list.rows.isEmpty else { return }
      self.list.scrollToItem(at: Int.random(in: 0..<self.list.rows.count), animated: false)
    }
    let more = UIBarButtonItem(image: UIImage(systemName: "ellipsis"), menu: UIMenu(options: .displayInline, children: [prepend, append, random]))
    var items = [more]
    if Settings.debug {
      // Left to right: prepend, append, random, More. UIKit puts the first item rightmost.
      items += [random, append, prepend].map { action in
        UIBarButtonItem(image: action.image, primaryAction: action)
      }
    }
    navigationItem.rightBarButtonItems = items
  }
}

// MARK: - Feed

final class FeedScreen: ListScreen {
  private let feed = ShadowListUtilsFeedList(theme: AppTheme.theme)
  private var generated = 0
  private var posts: [ShadowListUtilsFeedItem] = []
  private var loading = false

  override var prependTitle: String { "Publish New Posts" }
  override var appendTitle: String { "Load More Posts" }

  override func makeTemplate() -> ShadowListUtilsList? { feed }

  private func generate(_ count: Int) -> [ShadowListUtilsFeedItem] {
    defer { generated += count }
    return (generated..<generated + count).map { Items.feed(FeedPost(index: $0), states: Settings.states) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Feed"
    let count = Settings.count
    let seed = generate(max(1000, count))
    list.footer = ShadowListUtilsSpinner(theme: AppTheme.theme)
    list.onEndReached = { [weak self] in self?.appendRows() }
    installRefresh()
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.posts = Array(seed.prefix(count))
      self.show(.reset)
    }
  }

  /*
   * The template turns pull to refresh on with its closure. The other engines take a refresh
   * control of their own.
   */
  private func installRefresh() {
    if list.template != nil {
      feed.onRefresh = { [weak self] in self?.refresh() }
      return
    }
    let control = UIRefreshControl()
    control.tintColor = AppTheme.colors.secondaryLabel
    control.addTarget(self, action: #selector(refresh), for: .valueChanged)
    list.view.refreshControl = control
  }

  private func show(_ change: RowChange, animated: Bool = false) {
    show(
      posts, change, template: { feed.setItems($0, animated: animated) },
      row: { ShadowListUtilsFeedRow(item: $0, theme: AppTheme.theme, labels: ShadowListUtilsFeedLabels()) })
  }

  override func appendRows() {
    guard !loading, !posts.isEmpty else { return }
    loading = true
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.posts += self.generate(20)
      self.loading = false
      self.show(.append(20))
    }
  }

  override func prependRows() {
    posts = generate(10) + posts
    show(.prepend(10))
  }

  override func appendRowsNow() {
    posts += generate(20)
    show(.append(20))
  }

  @objc private func refresh() {
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      let count = Settings.count
      self.posts = Array((self.generate(10) + self.posts).prefix(count))
      self.show(.prepend(10))
      self.feed.isRefreshing = false
      self.list.view.refreshControl?.endRefreshing()
    }
  }

  /*
   * For the animate scenario: take out the post after the first visible one and put a new post
   * in after the next, in one animated change.
   */
  func animateChange(first: Int) {
    guard posts.count > first + 2 else { return }
    posts.remove(at: first + 1)
    posts.insert(Items.feed(FeedPost(index: 900_000)), at: first + 2)
    show(.update, animated: true)
  }
}

// MARK: - Chat

final class ChatScreen: ListScreen {
  private static let pageSize = 50
  private static let history = 1000

  private let chat = ShadowListUtilsChatList(theme: AppTheme.theme)
  private var all: [ChatMessage] = []
  private var firstLoaded = 0
  private var messages: [ShadowListUtilsChatMessage] = []
  private var captions: [String: String] = [:]
  private var nextForward = 1
  private var nextPrepended = 1
  private var loadingEarlier = false
  private let composer = ShadowListUtilsChatInput(theme: AppTheme.theme)

  override var prependTitle: String { "Load Earlier Messages" }
  override var appendTitle: String { "Simulate Incoming Messages" }
  override var inverted: Bool { true }

  override func makeTemplate() -> ShadowListUtilsList? { chat }

  override func layoutList() {
    composer.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(composer)
    composer.onSend = { [weak self] text in self?.send(text) }
    NSLayoutConstraint.activate([
      list.view.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
      list.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      list.view.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      list.view.bottomAnchor.constraint(equalTo: composer.topAnchor),
      composer.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      composer.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      composer.bottomAnchor.constraint(equalTo: view.keyboardLayoutGuide.topAnchor),
    ])
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Chat"
    let count = Settings.count
    all = (0..<Self.history + count).map { ChatMessage(index: $0) }
    let own = all.filter(\.isOwn)
    sendingId = own.last?.id
    failedId = own.dropLast().last?.id
    list.header = ShadowListUtilsListHeader(title: "Lisbon Crew", subtitle: "8 travellers · departs Oct 12", theme: AppTheme.theme)
    list.onStartReached = { [weak self] in self?.prependRows() }
    chat.captionForItem = { [weak self] in self?.captions[$0.key] }
    chat.onRetryMessage = { [weak self] in self?.retry($0) }
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.firstLoaded = self.all.count - count
      self.messages = self.all[self.firstLoaded...].map { self.item($0) }
      self.show(.reset)
    }
  }

  /*
   * A fixture message as the template's item. With -SLDebug 1 every message is captioned with
   * its place in the order it arrived. With -SLStates 1 every message whose index is 2 past a
   * multiple of 5 carries a caption, the newest of the reader's own messages is on its way and
   * the one before it failed.
   */
  private func item(_ message: ChatMessage, prepended: Bool = false) -> ShadowListUtilsChatMessage {
    if Settings.debug {
      if prepended {
        captions[message.id] = "prepended: " + Ordinals.label(nextPrepended)
        nextPrepended += 1
      } else {
        captions[message.id] = Ordinals.label(nextForward)
        nextForward += 1
      }
    }
    guard Settings.states else { return Items.chat(message) }
    if !Settings.debug, let index = message.index, index % Self.captionEvery == Self.captionOffset {
      captions[message.id] = "Edited"
    }
    let status: ShadowListUtilsChatMessageStatus? =
      message.id == sendingId ? .sending : message.id == failedId ? .failed : message.isOwn ? .sent : nil
    return Items.chat(message, status: status)
  }

  /*
   * The newest of the reader's own messages when the screen opened, and the one before it.
   */
  private var sendingId: String?
  private var failedId: String?

  private static let captionEvery = 5
  private static let captionOffset = 2

  private func show(_ change: RowChange) {
    show(
      messages, change, template: { chat.setItems($0, animated: false) },
      row: {
        ShadowListUtilsChatBubble(item: $0, caption: captions[$0.key], showsAuthor: true, theme: AppTheme.theme, labels: ShadowListUtilsChatLabels())
      })
    if firstLoaded == 0 && list.footer == nil {
      list.footer = ShadowListUtilsListFooter(text: "Start of the trip chat", theme: AppTheme.theme)
    }
  }

  override func prependRows() {
    guard !loadingEarlier, firstLoaded > 0, !messages.isEmpty else { return }
    loadingEarlier = true
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.loadingEarlier = false
      self.prependRowsNow()
    }
  }

  override func prependRowsNow() {
    let start = max(0, firstLoaded - Self.pageSize)
    // Numbered outward from the previous top row, which means walking the page bottom up.
    let page = all[start..<firstLoaded].reversed().map { item($0, prepended: true) }.reversed()
    messages = Array(page) + messages
    let added = firstLoaded - start
    firstLoaded = start
    show(.prepend(added))
  }

  override func appendRows() {
    guard !messages.isEmpty else { return }
    let start = all.count
    let incoming = (start..<start + 10).map { ChatMessage(index: $0) }
    all += incoming
    messages += incoming.map { item($0) }
    show(.append(10))
  }

  private func send(_ text: String) {
    let message = ChatMessage(id: "sent-\(UUID().uuidString)", author: Author("Me"), isOwn: true, text: text)
    messages.append(item(message))
    show(.append(1))
    list.scrollToEnd(animated: true)
  }

  /*
   * A failed message goes out again and arrives.
   */
  private func retry(_ message: ShadowListUtilsChatMessage) {
    guard let index = messages.firstIndex(where: { $0.key == message.key }) else { return }
    messages[index].status = .sent
    show(.update)
  }
}

// MARK: - Contacts

/*
 * A plain list of contacts: a tap opens the contact and a swipe removes it.
 */
final class ContactsScreen: ListScreen {
  private let companions = ShadowListUtilsContactsList(theme: AppTheme.theme)
  private var generated = 0
  private var contacts: [ShadowListUtilsContactItem] = []

  override var prependTitle: String { "Add Companions" }
  override var appendTitle: String { "Add More Companions" }

  override func makeTemplate() -> ShadowListUtilsList? { companions }

  private func generate(_ count: Int) -> [ShadowListUtilsContactItem] {
    defer { generated += count }
    return (generated..<generated + count).map { Items.contact(Contact(index: $0)) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Companions"
    companions.onPressItem = { [weak self] in self?.open($0) }
    companions.onDelete = { [weak self] in self?.delete($0) }
    let seed = generate(min(Settings.count, 200))
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.contacts = seed
      self.show(.reset)
    }
  }

  private func show(_ change: RowChange, animated: Bool = false) {
    show(
      contacts, change, template: { companions.setItems($0, animated: animated) },
      row: { ShadowListUtilsContactRow(item: $0, showsChevron: false, theme: AppTheme.theme) })
    list.footer = ShadowListUtilsListFooter(text: "\(contacts.count) companions", theme: AppTheme.theme)
  }

  override func prependRows() {
    contacts = generate(10) + contacts
    show(.prepend(10))
  }

  override func appendRows() {
    contacts += generate(10)
    show(.append(10))
  }

  private func open(_ contact: ShadowListUtilsContactItem) {
    navigationController?.pushViewController(ContactScreen(contact), animated: true)
  }

  private func delete(_ contact: ShadowListUtilsContactItem) {
    contacts.removeAll { $0.key == contact.key }
    show(.update, animated: true)
  }
}

/*
 * One contact, shown with the library's empty state: a large avatar over the name.
 */
final class ContactScreen: UIViewController {
  private let state: ShadowListUtilsEmptyState

  init(_ contact: ShadowListUtilsContactItem) {
    let avatar = ShadowListUtilsAvatar(size: 96, theme: AppTheme.theme)
    avatar.show(name: contact.name, imageUrl: contact.avatarUrl, color: contact.avatarColor)
    state = ShadowListUtilsEmptyState(title: contact.name, message: contact.subtitle, icon: avatar, theme: AppTheme.theme)
    super.init(nibName: nil, bundle: nil)
    title = contact.name
    navigationItem.largeTitleDisplayMode = .never
  }

  required init?(coder: NSCoder) { fatalError() }

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = AppTheme.colors.background
    view.addSubview(state)
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    let area = view.bounds.inset(by: view.safeAreaInsets)
    let size = state.sizeThatFits(CGSize(width: area.width, height: .greatestFiniteMagnitude))
    state.frame = CGRect(x: area.minX, y: area.minY, width: area.width, height: size.height)
  }
}

// MARK: - Directory

/*
 * Contacts in sections by the first letter of their name. The template shows them with the
 * kit's sections, sticky headers and section index. The other engines show the same rows flat,
 * with the header rows stuck by index and a rail of the example's own.
 */
final class DirectoryScreen: ListScreen {
  private let directory = ShadowListUtilsContactsSectionList(theme: AppTheme.theme)
  private var generated = 0
  private var contacts: [ShadowListUtilsContactItem] = []
  private var headerIndices: [Int] = []
  private let rail = SectionIndexRail()

  override var prependTitle: String { "Add Travellers" }
  override var appendTitle: String { "Add More Travellers" }

  override func makeTemplate() -> ShadowListUtilsList? { directory }

  private func generate(_ count: Int) -> [ShadowListUtilsContactItem] {
    defer { generated += count }
    return (generated..<generated + count).map { Items.contact(Contact(index: $0)) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Directory"
    if list.template == nil {
      installRail()
    }
    let seed = generate(Settings.count)
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.contacts = seed
      self.show(.reset)
    }
  }

  private func installRail() {
    rail.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(rail)
    NSLayoutConstraint.activate([
      rail.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      rail.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor),
      rail.widthAnchor.constraint(equalToConstant: 24),
    ])
    rail.onSelect = { [weak self] section in
      guard let self, section < self.headerIndices.count else { return }
      self.list.scrollToItem(at: self.headerIndices[section], animated: false)
    }
  }

  private func show(_ change: RowChange) {
    // A stable sort by name. The library groups them and keeps this order inside a section.
    let sorted = contacts.enumerated().sorted {
      let order = $0.element.name.localizedCompare($1.element.name)
      return order == .orderedSame ? $0.offset < $1.offset : order == .orderedAscending
    }.map(\.element)
    let sections = shadowListUtilsGroupIntoSections(sorted)
    list.footer = ShadowListUtilsListFooter(text: "\(contacts.count) travellers", theme: AppTheme.theme)
    if list.template != nil {
      list.update { directory.setSections(sections, animated: false) }
      directory.sectionIndexTitles = sections.map(\.title)
      return
    }
    var rows: [ShadowListUtilsRow] = []
    headerIndices = []
    for section in sections {
      headerIndices.append(rows.count)
      rows.append(ShadowListUtilsSectionHeader(key: "section-\(section.key)", title: section.title, count: section.items.count, theme: AppTheme.theme))
      rows += section.items.map { ShadowListUtilsContactRow(item: $0, showsChevron: false, theme: AppTheme.theme) }
    }
    list.stickyIndices = headerIndices
    list.setRows(rows, change: change)
    rail.titles = sections.map(\.title)
  }

  override func prependRows() {
    contacts = generate(10) + contacts
    show(.update)
  }

  override func appendRows() {
    contacts += generate(10)
    show(.update)
  }
}

final class SectionIndexRail: UIView {
  var onSelect: ((Int) -> Void)?
  var titles: [String] = [] {
    didSet { rebuild() }
  }

  private let stack = UIStackView()
  private var current = -1

  override init(frame: CGRect) {
    super.init(frame: frame)
    stack.axis = .vertical
    stack.alignment = .center
    stack.translatesAutoresizingMaskIntoConstraints = false
    addSubview(stack)
    NSLayoutConstraint.activate([
      stack.topAnchor.constraint(equalTo: topAnchor, constant: 6),
      stack.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -6),
      stack.leadingAnchor.constraint(equalTo: leadingAnchor),
      stack.trailingAnchor.constraint(equalTo: trailingAnchor),
    ])
  }

  required init?(coder: NSCoder) { fatalError() }

  private func rebuild() {
    stack.arrangedSubviews.forEach { $0.removeFromSuperview() }
    for title in titles {
      let label = UILabel()
      label.attributedText = NSAttributedString(string: title, attributes: [
        .font: UIFont.systemFont(ofSize: 11, weight: .semibold), .foregroundColor: AppTheme.colors.accent,
      ])
      label.heightAnchor.constraint(equalToConstant: 14).isActive = true
      stack.addArrangedSubview(label)
    }
  }

  override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) { track(touches) }
  override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) { track(touches) }

  private func track(_ touches: Set<UITouch>) {
    guard let touch = touches.first, !titles.isEmpty else { return }
    let y = touch.location(in: stack).y
    let index = min(titles.count - 1, max(0, Int(y / max(1, stack.bounds.height) * CGFloat(titles.count))))
    if index != current {
      current = index
      UISelectionFeedbackGenerator().selectionChanged()
      onSelect?(index)
    }
  }

  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { current = -1 }
}

// MARK: - Gallery

final class GalleryScreen: ListScreen {
  private let gallery = ShadowListUtilsMasonryList(theme: AppTheme.theme)
  private var generated = 0
  private var photos: [ShadowListUtilsMasonryItem] = []
  private var loading = false

  override var prependTitle: String { "Publish New Photos" }
  override var appendTitle: String { "Load More Photos" }
  override var columns: Int { 3 }

  override func makeTemplate() -> ShadowListUtilsList? { gallery }

  private func generate(_ count: Int) -> [ShadowListUtilsMasonryItem] {
    defer { generated += count }
    return (generated..<generated + count).map { Items.photo(Photo(index: $0)) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Gallery"
    let count = Settings.count
    let seed = generate(max(100, count))
    list.footer = ShadowListUtilsSpinner(theme: AppTheme.theme)
    list.onEndReached = { [weak self] in self?.appendRows() }
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.photos = Array(seed.prefix(count))
      self.show(.reset)
    }
  }

  private func show(_ change: RowChange) {
    show(
      photos, change, template: { gallery.setItems($0, animated: false) },
      row: { ShadowListUtilsMasonryCard(item: $0, theme: AppTheme.theme, labels: ShadowListUtilsMasonryLabels()) })
  }

  override func appendRows() {
    guard !loading, !photos.isEmpty else { return }
    loading = true
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.photos += self.generate(30)
      self.loading = false
      self.show(.append(30))
    }
  }

  override func prependRows() {
    photos = generate(10) + photos
    show(.prepend(10))
  }
}

// MARK: - Home

struct ExampleRoute {
  let route: String
  let title: String
  let summary: String
  let make: () -> UIViewController

  static let all: [ExampleRoute] = [
    ExampleRoute(route: "Feed", title: "Feed", summary: "Posts with photo carousels and pull to refresh") { FeedScreen() },
    ExampleRoute(route: "Masonry", title: "Gallery", summary: "Photos in three columns") { GalleryScreen() },
    ExampleRoute(route: "Nested", title: "Explore", summary: "Horizontal shelves inside a vertical list") { NestedScreen() },
    ExampleRoute(route: "Chat", title: "Chat", summary: "Group chat that keeps your place as messages arrive") { ChatScreen() },
    ExampleRoute(route: "Contacts", title: "Companions", summary: "Swipe a row to remove it") { ContactsScreen() },
    ExampleRoute(route: "SectionList", title: "Directory", summary: "Sections with sticky headers and an index") { DirectoryScreen() },
    ExampleRoute(route: "Reorder", title: "Boarding Order", summary: "Touch and hold a row, then drag it") { ReorderScreen() },
    ExampleRoute(route: "ReorderGrid", title: "Sky Wishlist", summary: "Touch and hold a card, then drag it across the grid") { ReorderGridScreen() },
    ExampleRoute(route: "Snap", title: "Destinations", summary: "Cards that snap into place") { SnapScreen() },
    ExampleRoute(route: "Sections", title: "Sections", summary: "Section headers, footers, an index and self-sizing cells") { SectionsScreen() },
    ExampleRoute(route: "Assistant", title: "Assistant", summary: "A controlled transcript, live replies and composer actions") { AssistantScreen() },
    ExampleRoute(route: "Activity", title: "Activity", summary: "Unread activity and pull to refresh") { ActivityScreen() },
    ExampleRoute(route: "Board", title: "Travel board", summary: "Move cards within and between columns") { BoardScreen() },
    ExampleRoute(route: "Tree", title: "Files", summary: "Expand folders and open files") { TreeScreen() },
    ExampleRoute(route: "Poll", title: "Poll", summary: "Choose a destination and see the votes") { PollScreen() },
    ExampleRoute(route: "Inbox", title: "Inbox", summary: "Swipe actions, a menu on long press and selection") { InboxScreen() },
  ]
}

final class HomeScreen: UITableViewController {
  init() {
    super.init(style: .insetGrouped)
    title = "ShadowList UIKit"
  }

  required init?(coder: NSCoder) { fatalError() }

  override func viewDidLoad() {
    super.viewDidLoad()
    navigationItem.largeTitleDisplayMode = .always
    tableView.register(UITableViewCell.self, forCellReuseIdentifier: "cell")
  }

  override func numberOfSections(in tableView: UITableView) -> Int { 2 }

  override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
    section == 0 ? "Engine" : "Examples"
  }

  override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    section == 0 ? Engine.allCases.count : ExampleRoute.all.count
  }

  override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    let cell = tableView.dequeueReusableCell(withIdentifier: "cell", for: indexPath)
    var content = UIListContentConfiguration.subtitleCell()
    if indexPath.section == 0 {
      let engine = Engine.allCases[indexPath.row]
      content.text = engine.title
      cell.accessoryType = engine == Settings.engine ? .checkmark : .none
    } else {
      let example = ExampleRoute.all[indexPath.row]
      content.text = example.title
      content.secondaryText = example.summary
      cell.accessoryType = .disclosureIndicator
    }
    cell.contentConfiguration = content
    return cell
  }

  override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
    tableView.deselectRow(at: indexPath, animated: true)
    if indexPath.section == 0 {
      UserDefaults.standard.set(Engine.allCases[indexPath.row].rawValue, forKey: "SLEngine")
      tableView.reloadSections(IndexSet(integer: 0), with: .none)
    } else {
      navigationController?.pushViewController(ExampleRoute.all[indexPath.row].make(), animated: true)
    }
  }
}
