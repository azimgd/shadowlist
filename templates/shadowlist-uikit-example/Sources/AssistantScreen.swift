import ShadowListUtils
import UIKit

/*
 * The same travel assistant flow as the RN example. Scripted data belongs to this app; the
 * native views only display messages and request edits, votes, variants, stops and retries.
 */
final class AssistantScreen: UIViewController {
  let list = ShadowListUtilsAssistantList(theme: AppTheme.theme)
  let composer = ShadowListUtilsAssistantComposer(theme: AppTheme.theme)
  private let welcome = ShadowListUtilsAssistantEmpty(theme: AppTheme.theme)
  private let welcomeFrame = UIView()
  private var items: [ShadowListUtilsAssistantMessage] = []
  private var turns: [String: ShadowListUtilsAssistantTurn] = [:]
  private var pending: DispatchWorkItem?
  private var activeKey: String?
  private var editingKey: String?
  private var sequence = 0
  private let models = ["Cruise", "Jet", "Long-haul"]
  private var modelIndex = 0

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Assistant"
    navigationItem.rightBarButtonItem = UIBarButtonItem(image: UIImage(systemName: "ellipsis"), menu: UIMenu(children: [
      UIAction(title: "Load Earlier Messages", image: UIImage(systemName: "arrow.up.to.line")) { [weak self] _ in
        self?.list.scrollToItem(at: 0, viewPosition: 0, animated: false)
      },
      UIAction(title: "Send Next Suggestion", image: UIImage(systemName: "arrow.down.to.line")) { [weak self] _ in
        guard let self, !self.welcome.suggestions.isEmpty else { return }
        self.send(self.welcome.suggestions[self.sequence % self.welcome.suggestions.count].prompt, attachments: [])
      },
      UIAction(title: "Jump to Random Item", image: UIImage(systemName: "scope")) { [weak self] _ in
        guard let self, !self.items.isEmpty else { return }
        self.list.scrollToItem(at: Int.random(in: self.items.indices), viewPosition: 0, animated: false)
      },
    ]))
    view.backgroundColor = AppTheme.colors.background
    list.translatesAutoresizingMaskIntoConstraints = false
    composer.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(list)
    view.addSubview(composer)
    // The composer runs to the screen bottom and pads itself by the safe area, or above a keyboard.
    if #available(iOS 17, *) { view.keyboardLayoutGuide.usesBottomSafeArea = false }
    NSLayoutConstraint.activate([
      list.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
      list.leadingAnchor.constraint(equalTo: view.leadingAnchor), list.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      list.bottomAnchor.constraint(equalTo: composer.topAnchor),
      composer.leadingAnchor.constraint(equalTo: view.leadingAnchor), composer.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      composer.bottomAnchor.constraint(equalTo: view.keyboardLayoutGuide.topAnchor),
    ])
    composer.model = models[modelIndex]
    composer.onSend = { [weak self] text, attachments in self?.send(text, attachments: attachments) }
    composer.onStop = { [weak self] in self?.stop() }
    composer.onThinkingChange = { [weak self] in self?.composer.thinking = $0 }
    composer.onPressModel = { [weak self] in
      guard let self else { return }
      self.modelIndex = (self.modelIndex + 1) % self.models.count
      self.composer.model = self.models[self.modelIndex]
    }
    composer.onPressAttach = { [weak self] in
      self?.composer.attachments.append(ShadowListUtilsAssistantAttachment(key: UUID().uuidString, kind: .file,
        name: "Boarding pass.pdf", detail: "PDF · 84 KB"))
    }
    composer.onRemoveAttachment = { [weak self] key in self?.composer.attachments.removeAll { $0.key == key } }
    composer.onCancelEdit = { [weak self] in self?.editingKey = nil; self?.composer.editing = false; self?.composer.clearDraft() }
    list.onCopy = { UIPasteboard.general.string = $0 }
    list.onCopyCode = { UIPasteboard.general.string = $0 }
    list.onShare = { [weak self] text in
      guard let self else { return }
      let controller = UIActivityViewController(activityItems: [text], applicationActivities: nil)
      controller.popoverPresentationController?.sourceView = self.composer
      self.present(controller, animated: true)
    }
    list.onFollowUp = { [weak self] in self?.send($0, attachments: []) }
    list.onSelectVariant = { [weak self] key, index in
      self?.updateReply(key) { if $0.variants.indices.contains(index) { $0.variantIndex = index } }
      self?.show()
    }
    list.onFeedback = { [weak self] key, feedback in self?.updateReply(key) { $0.feedback = feedback }; self?.show() }
    list.onRegenerate = { [weak self] key in self?.regenerate(key) }
    list.onRetry = { [weak self] key in self?.beginReply(key: key, fail: false) }
    list.onEdit = { [weak self] key in
      guard let self, let item = self.items.first(where: { $0.key == key }), case let .user(prompt) = item else { return }
      self.editingKey = key
      self.composer.editing = true
      self.composer.setDraft(prompt.text)
    }
    welcome.labels.emptyTitle = "Where are we flying?"
    welcome.labels.emptySubtitle = "Plan flights, weather and stays."
    welcome.suggestions = [
      ShadowListUtilsAssistantSuggestion(title: "Write an itinerary", prompt: "Write a three-day itinerary for Lisbon in May"),
      ShadowListUtilsAssistantSuggestion(title: "Compare fares", prompt: "Compare fares to Tokyo across airlines in a table"),
      ShadowListUtilsAssistantSuggestion(title: "Search flights", prompt: "Search Skyfy for flights to Reykjavik"),
      ShadowListUtilsAssistantSuggestion(title: "Try a dropped connection", prompt: "Simulate a dropped connection error"),
    ]
    welcome.onSelectSuggestion = { [weak self] in self?.send($0.prompt, attachments: []) }
    // The welcome shows in the list's frame, which cuts it off at the composer.
    welcome.translatesAutoresizingMaskIntoConstraints = false
    welcomeFrame.translatesAutoresizingMaskIntoConstraints = false
    welcomeFrame.clipsToBounds = true
    welcomeFrame.addSubview(welcome)
    view.addSubview(welcomeFrame)
    NSLayoutConstraint.activate([
      welcomeFrame.leadingAnchor.constraint(equalTo: list.leadingAnchor),
      welcomeFrame.trailingAnchor.constraint(equalTo: list.trailingAnchor),
      welcomeFrame.topAnchor.constraint(equalTo: list.topAnchor),
      welcomeFrame.bottomAnchor.constraint(equalTo: list.bottomAnchor),
      welcome.leadingAnchor.constraint(equalTo: welcomeFrame.leadingAnchor),
      welcome.trailingAnchor.constraint(equalTo: welcomeFrame.trailingAnchor),
      welcome.topAnchor.constraint(equalTo: welcomeFrame.topAnchor),
    ])
    composer.thinking = true
    show()
  }

  deinit { pending?.cancel() }

  private func send(_ text: String, attachments: [ShadowListUtilsAssistantAttachment]) {
    guard activeKey == nil else { return }
    sequence += 1
    if let editingKey, let index = items.firstIndex(where: { $0.key == editingKey }) {
      items = Array(items.prefix(index)) + [.user(ShadowListUtilsAssistantPrompt(key: editingKey, text: text, attachments: attachments))]
      self.editingKey = nil
      composer.editing = false
    } else {
      items.removeAll { if case .end = $0 { return true }; return false }
      items.append(.user(ShadowListUtilsAssistantPrompt(key: "prompt-\(sequence)", text: text, attachments: attachments)))
    }
    let key = "reply-\(sequence)"
    items += [.assistant(ShadowListUtilsAssistantReply(key: key, variants: [ShadowListUtilsAssistantTurn()], model: composer.model)), .endMarker]
    composer.attachments = []
    beginReply(key: key, fail: text.localizedCaseInsensitiveContains("error"))
  }

  private func regenerate(_ key: String) {
    guard activeKey == nil else { return }
    updateReply(key) { reply in reply.variants.append(ShadowListUtilsAssistantTurn()); reply.variantIndex = reply.variants.count - 1 }
    beginReply(key: key, fail: false)
  }

  private func beginReply(key: String, fail: Bool) {
    pending?.cancel()
    activeKey = key
    composer.streaming = true
    list.streaming = true
    turns[key] = ShadowListUtilsAssistantTurn(thinking: "Checking the itinerary and comparing the options.")
    show()
    advance(key: key, step: 0, fail: fail)
  }

  private func advance(key: String, step: Int, fail: Bool) {
    let content = "# Three days in Lisbon\n\nStart with **Alfama** and the riverfront, then leave time for the trams.\n\n| Day | Plan |\n| --- | --- |\n| 1 | Alfama |\n| 2 | Belém |\n| 3 | Sintra |\n\n```text\nPack a small bag and comfortable shoes.\n```\n\n[Visit Lisbon](https://www.visitlisboa.com)"
    let lengths = [0, content.utf16.count / 3, content.utf16.count * 2 / 3, content.utf16.count]
    let work = DispatchWorkItem { [weak self] in
      guard let self, self.activeKey == key else { return }
      let units = Array(content.utf16.prefix(lengths[step]))
      var turn = ShadowListUtilsAssistantTurn(content: String(decoding: units, as: UTF16.self),
        thinking: "Checking the itinerary and comparing the options.", thinkingMs: step == 0 ? nil : 800,
        toolCalls: [ShadowListUtilsAssistantToolCall(key: "search-\(key)", name: "Search flights", input: "Lisbon in May", at: 0,
          status: step < 2 ? .running : .done(output: "Found three useful routes"))])
      if fail && step == 2 { turn.status = .failed(error: "Connection dropped. The partial reply is kept; retry continues in this message.") }
      else if step == 3 {
        turn.status = .done(sources: [ShadowListUtilsAssistantSource(key: "lisbon", title: "Visit Lisbon", url: "https://www.visitlisboa.com")],
          followUps: ["Why fly midweek?", "Set a fare alert"])
      }
      self.turns[key] = turn
      if step == 3 || (fail && step == 2) { self.finish(key: key, turn: turn) }
      else { self.show(); self.advance(key: key, step: step + 1, fail: fail) }
    }
    pending = work
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.6, execute: work)
  }

  private func stop() {
    guard let key = activeKey, var turn = turns[key] else { return }
    pending?.cancel()
    turn.status = .stopped
    finish(key: key, turn: turn)
  }

  private func finish(key: String, turn: ShadowListUtilsAssistantTurn) {
    updateReply(key) { reply in
      if reply.variants.indices.contains(reply.variantIndex) { reply.variants[reply.variantIndex] = turn }
    }
    turns.removeValue(forKey: key)
    activeKey = nil
    composer.streaming = false
    list.streaming = false
    show()
  }

  private func updateReply(_ key: String, update: (inout ShadowListUtilsAssistantReply) -> Void) {
    guard let index = items.firstIndex(where: { $0.key == key }), case var .assistant(reply) = items[index] else { return }
    update(&reply)
    items[index] = .assistant(reply)
  }

  private func show() {
    welcomeFrame.isHidden = !items.isEmpty
    list.items = items
    list.turns = turns
  }
}
