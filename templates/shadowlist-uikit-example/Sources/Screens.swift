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
 * add rows at the end and jump to a random row.
 */
class ListScreen: UIViewController {
  let engine = Settings.engine
  var list: ListController!

  func makeList() -> ListController { ListController(engine: engine) }
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
    view.backgroundColor = Theme.background
    list = makeList()
    list.view.translatesAutoresizingMaskIntoConstraints = false
    list.view.accessibilityIdentifier = "list"
    view.addSubview(list.view)
    layoutList()
    installActions()
    let opaque = UINavigationBarAppearance()
    opaque.configureWithOpaqueBackground()
    opaque.backgroundColor = Theme.background
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
  private var generated = 0
  private var posts: [FeedPost] = []
  private var loading = false

  override var prependTitle: String { "Publish New Posts" }
  override var appendTitle: String { "Load More Posts" }

  private func generate(_ count: Int) -> [FeedPost] {
    defer { generated += count }
    return (generated..<generated + count).map { FeedPost(index: $0) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Feed"
    let count = Settings.count
    let seed = generate(max(1000, count))
    list.footer = SpinnerView()
    list.onEndReached = { [weak self] in self?.appendRows() }
    let refresh = UIRefreshControl()
    refresh.tintColor = Theme.secondaryLabel
    refresh.addTarget(self, action: #selector(refreshPulled), for: .valueChanged)
    list.view.refreshControl = refresh
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.posts = Array(seed.prefix(count))
      self.show(.reset)
    }
  }

  private func show(_ change: RowChange) {
    list.setRows(posts.map { FeedRow(post: $0) }, change: change)
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

  @objc private func refreshPulled() {
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      let count = Settings.count
      self.posts = Array((self.generate(10) + self.posts).prefix(count))
      self.show(.prepend(10))
      self.list.view.refreshControl?.endRefreshing()
    }
  }
}

// MARK: - Chat

final class ChatScreen: ListScreen {
  private static let pageSize = 50
  private static let history = 1000

  private var all: [ChatMessage] = []
  private var firstLoaded = 0
  private var messages: [ChatMessage] = []
  private var nextForward = 1
  private var nextPrepended = 1
  private var loadingEarlier = false
  private let composer = ChatComposer()

  override var prependTitle: String { "Load Earlier Messages" }
  override var appendTitle: String { "Simulate Incoming Messages" }

  override func makeList() -> ListController { ListController(engine: engine, inverted: true) }

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
    list.header = ListHeaderView(title: "Lisbon Crew", subtitle: "8 travellers · departs Oct 12")
    list.onStartReached = { [weak self] in self?.prependRows() }
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.firstLoaded = self.all.count - count
      self.messages = self.all[self.firstLoaded...].map { self.numbered($0) }
      self.show(.reset)
    }
  }

  private func numbered(_ message: ChatMessage, prepended: Bool = false) -> ChatMessage {
    guard Settings.debug else { return message }
    var message = message
    if prepended {
      message.caption = "prepended: " + Ordinals.label(nextPrepended)
      nextPrepended += 1
    } else {
      message.caption = Ordinals.label(nextForward)
      nextForward += 1
    }
    return message
  }

  private func show(_ change: RowChange) {
    list.setRows(messages.map { ChatRow(message: $0, caption: $0.caption) }, change: change)
    if firstLoaded == 0 && list.footer == nil {
      list.footer = ListFooterView("Start of the trip chat")
    }
  }

  override func prependRows() {
    guard !loadingEarlier, firstLoaded > 0, !messages.isEmpty else { return }
    loadingEarlier = true
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      let start = max(0, self.firstLoaded - Self.pageSize)
      // Numbered outward from the previous top row, which means walking the page bottom up.
      let page = self.all[start..<self.firstLoaded].reversed().map { self.numbered($0, prepended: true) }.reversed()
      self.messages = Array(page) + self.messages
      let added = self.firstLoaded - start
      self.firstLoaded = start
      self.loadingEarlier = false
      self.show(.prepend(added))
    }
  }

  override func prependRowsNow() {
    let start = max(0, firstLoaded - Self.pageSize)
    let page = all[start..<firstLoaded].reversed().map { numbered($0, prepended: true) }.reversed()
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
    messages += incoming.map { numbered($0) }
    show(.append(10))
  }

  private func send(_ text: String) {
    let message = ChatMessage(id: "sent-\(UUID().uuidString)", author: Author("Me"), isOwn: true, text: text)
    messages.append(numbered(message))
    show(.append(1))
    list.scrollToEnd(animated: true)
  }
}

final class ChatComposer: UIView, UITextViewDelegate {
  var onSend: ((String) -> Void)?
  private let box = UIView()
  private let input = UITextView()
  private let placeholder = UILabel()
  private let send = UIButton(type: .system)
  private var inputHeight: NSLayoutConstraint!

  override init(frame: CGRect) {
    super.init(frame: frame)
    backgroundColor = Theme.background
    let border = UIView()
    border.backgroundColor = Theme.separator
    box.layer.cornerRadius = 18
    box.layer.borderWidth = Theme.hairline
    box.layer.borderColor = Theme.separator.cgColor
    input.font = TextStyle.body.font
    input.textColor = Theme.label
    input.backgroundColor = .clear
    input.textContainerInset = .zero
    input.textContainer.lineFragmentPadding = 0
    input.delegate = self
    placeholder.attributedText = TextStyle.body.string("Message your crew", Theme.secondaryLabel)
    send.setImage(UIImage(systemName: "arrow.up", withConfiguration: UIImage.SymbolConfiguration(pointSize: 16, weight: .bold)), for: .normal)
    send.layer.cornerRadius = 16
    send.addTarget(self, action: #selector(sendTapped), for: .touchUpInside)
    for subview in [border, box, input, placeholder, send] {
      subview.translatesAutoresizingMaskIntoConstraints = false
      addSubview(subview)
    }
    inputHeight = input.heightAnchor.constraint(equalToConstant: 22)
    NSLayoutConstraint.activate([
      border.topAnchor.constraint(equalTo: topAnchor),
      border.leadingAnchor.constraint(equalTo: leadingAnchor),
      border.trailingAnchor.constraint(equalTo: trailingAnchor),
      border.heightAnchor.constraint(equalToConstant: Theme.hairline),
      box.topAnchor.constraint(equalTo: topAnchor, constant: 8),
      box.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 8),
      box.trailingAnchor.constraint(equalTo: send.leadingAnchor, constant: -8),
      box.bottomAnchor.constraint(equalTo: safeAreaLayoutGuide.bottomAnchor, constant: -8),
      input.topAnchor.constraint(equalTo: box.topAnchor, constant: 7),
      input.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -7),
      input.leadingAnchor.constraint(equalTo: box.leadingAnchor, constant: 12),
      input.trailingAnchor.constraint(equalTo: box.trailingAnchor, constant: -12),
      inputHeight,
      placeholder.leadingAnchor.constraint(equalTo: input.leadingAnchor),
      placeholder.centerYAnchor.constraint(equalTo: input.centerYAnchor),
      send.widthAnchor.constraint(equalToConstant: 32),
      send.heightAnchor.constraint(equalToConstant: 32),
      send.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -8),
      send.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -2),
    ])
    updateSend()
  }

  required init?(coder: NSCoder) { fatalError() }

  func textViewDidChange(_ textView: UITextView) {
    let height = min(106, max(22, ceil(textView.sizeThatFits(CGSize(width: textView.bounds.width, height: .greatestFiniteMagnitude)).height)))
    inputHeight.constant = height
    updateSend()
  }

  private func updateSend() {
    let enabled = !input.text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    placeholder.isHidden = !input.text.isEmpty
    send.backgroundColor = enabled ? Theme.accent : Theme.fill
    send.tintColor = enabled ? Theme.onAccent : Theme.secondaryLabel
    send.isEnabled = enabled
  }

  @objc private func sendTapped() {
    let text = input.text.trimmingCharacters(in: .whitespacesAndNewlines)
    guard !text.isEmpty else { return }
    input.text = ""
    textViewDidChange(input)
    UIImpactFeedbackGenerator(style: .light).impactOccurred()
    onSend?(text)
  }
}

// MARK: - Directory

final class DirectoryScreen: ListScreen {
  private var generated = 0
  private var contacts: [Contact] = []
  private var headerIndices: [Int] = []
  private var titles: [String] = []
  private let rail = SectionIndexRail()

  override var prependTitle: String { "Add Travellers" }
  override var appendTitle: String { "Add More Travellers" }

  private func generate(_ count: Int) -> [Contact] {
    defer { generated += count }
    return (generated..<generated + count).map { Contact(index: $0) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Directory"
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
    let seed = generate(Settings.count)
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.contacts = seed
      self.show(.reset)
    }
  }

  private func show(_ change: RowChange) {
    var groups: [String: [Contact]] = [:]
    for contact in contacts {
      let title = contact.author.name.first.map { String($0).uppercased() } ?? "#"
      groups[title, default: []].append(contact)
    }
    titles = groups.keys.sorted { $0.localizedCompare($1) == .orderedAscending }
    var rows: [Row] = []
    headerIndices = []
    for title in titles {
      // A stable sort. Equal names keep their order.
      let members = groups[title]!.enumerated().sorted {
        let order = $0.element.author.name.localizedCompare($1.element.author.name)
        return order == .orderedSame ? $0.offset < $1.offset : order == .orderedAscending
      }.map(\.element)
      headerIndices.append(rows.count)
      rows.append(SectionHeaderRow(title: title, count: members.count))
      for (position, contact) in members.enumerated() {
        rows.append(ContactRow(contact: contact, separatorBelow: position < members.count - 1))
      }
    }
    list.footer = ListFooterView("\(contacts.count) travellers")
    list.stickyIndices = headerIndices
    list.setRows(rows, change: change)
    rail.titles = titles
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
        .font: UIFont.systemFont(ofSize: 11, weight: .semibold), .foregroundColor: Theme.accent,
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
  private var generated = 0
  private var photos: [Photo] = []
  private var loading = false

  override var prependTitle: String { "Publish New Photos" }
  override var appendTitle: String { "Load More Photos" }

  override func makeList() -> ListController { ListController(engine: engine, columns: 3) }

  private func generate(_ count: Int) -> [Photo] {
    defer { generated += count }
    return (generated..<generated + count).map { Photo(index: $0) }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Gallery"
    let count = Settings.count
    let seed = generate(max(100, count))
    list.footer = SpinnerView()
    list.onEndReached = { [weak self] in self?.appendRows() }
    FakeNetwork.reply { [weak self] in
      guard let self else { return }
      self.photos = Array(seed.prefix(count))
      self.show(.reset)
    }
  }

  private func show(_ change: RowChange) {
    list.setRows(photos.map { PhotoRow(photo: $0) }, change: change)
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
    ExampleRoute(route: "Chat", title: "Chat", summary: "Group chat that keeps your place as messages arrive") { ChatScreen() },
    ExampleRoute(route: "SectionList", title: "Directory", summary: "Sections with sticky headers and an index") { DirectoryScreen() },
    ExampleRoute(route: "Reorder", title: "Boarding Order", summary: "Touch and hold a row, then drag it") { ReorderScreen() },
    ExampleRoute(route: "Snap", title: "Destinations", summary: "Cards that snap into place") { SnapScreen() },
    ExampleRoute(route: "Sections", title: "Sections", summary: "Section headers, footers, an index and self-sizing cells") { SectionsScreen() },
    ExampleRoute(route: "Inbox", title: "Inbox", summary: "Swipe actions, menus, selection, refresh and batch updates") { InboxScreen() },
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
