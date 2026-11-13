import UIKit

/*
 * A controlled transcript. The app owns messages, saved variants and live turns; the list only
 * asks for actions. End markers remain real rows and are excluded from anchoring by metadata.
 */
public final class ShadowListUtilsAssistantList: ShadowListUtilsList {
  public var items: [ShadowListUtilsAssistantMessage] {
    get { storedItems }
    set { setItems(newValue, animated: false) }
  }
  public func setItems(_ items: [ShadowListUtilsAssistantMessage], animated: Bool) {
    storedItems = items
    let keys = Set(items.map(\.key))
    actionsVisibleKeys.formIntersection(keys)
    thinkingExpandedKeys.formIntersection(keys)
    expandedToolKeys = expandedToolKeys.filter { keys.contains($0.key) }
    let next = Set(items.compactMap { item -> String? in if case let .end(key) = item { return key }; return nil })
    nonAnchorKeys = (nonAnchorKeys ?? []).subtracting(endKeys).union(next)
    endKeys = next
    rebuildRows(animated: animated)
  }
  public var turns: [String: ShadowListUtilsAssistantTurn] {
    get { storedTurns }
    set { setTurns(newValue, animated: false) }
  }
  public func setTurns(_ turns: [String: ShadowListUtilsAssistantTurn], animated: Bool) {
    storedTurns = turns
    rebuildRows(animated: animated)
  }
  public var streaming = false { didSet { reloadRows() } }
  public var rowForItem: ((ShadowListUtilsAssistantMessage) -> ShadowListUtilsRow)? { didSet { reloadRows() } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { reloadRows() } }
  public var onCopy: ((String) -> Void)? { didSet { reloadRows() } }
  public var onCopyCode: ((String) -> Void)? { didSet { reloadRows() } }
  public var onShare: ((String) -> Void)? { didSet { reloadRows() } }
  public var onRegenerate: ((String) -> Void)? { didSet { reloadRows() } }
  public var onRetry: ((String) -> Void)? { didSet { reloadRows() } }
  public var onSelectVariant: ((String, Int) -> Void)? { didSet { reloadRows() } }
  public var onFeedback: ((String, ShadowListUtilsAssistantFeedback?) -> Void)? { didSet { reloadRows() } }
  public var onFollowUp: ((String) -> Void)? { didSet { reloadRows() } }
  public var onOpenLink: ((String) -> Void)? { didSet { reloadRows() } }
  public var onEdit: ((String) -> Void)? { didSet { reloadRows() } }
  private var storedItems: [ShadowListUtilsAssistantMessage] = []
  private var storedTurns: [String: ShadowListUtilsAssistantTurn] = [:]
  private var actionsVisibleKeys: Set<String> = []
  private var thinkingExpandedKeys: Set<String> = []
  private var expandedToolKeys: [String: Set<String>] = [:]
  private var endKeys: Set<String> = []

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    isInverted = true
    followAppends = true
    keyboardDismissMode = .onDrag
    allowsSelection = false
  }

  override func makeRows() -> [ShadowListUtilsRow]? {
    let latestKey = storedItems.count >= 2 ? storedItems[storedItems.count - 2].key : nil
    let handlers = ShadowListUtilsAssistantHandlers(onCopy: onCopy, onCopyCode: onCopyCode, onShare: onShare,
      onRegenerate: onRegenerate, onRetry: onRetry, onSelectVariant: onSelectVariant, onFeedback: onFeedback,
      onFollowUp: onFollowUp, onOpenLink: onOpenLink, onEdit: onEdit)
    return storedItems.map { item in
      rowForItem?(item) ?? ShadowListUtilsAssistantRow(item: item, turn: storedTurns[item.key], busy: streaming,
        isLatest: item.key == latestKey, actionsVisible: actionsVisibleKeys.contains(item.key),
        thinkingExpanded: thinkingExpandedKeys.contains(item.key), expandedToolKeys: expandedToolKeys[item.key] ?? [],
        theme: theme, labels: labels, handlers: handlers,
        onActionsChange: { [weak self] key, visible in
          guard let self else { return }
          if visible { self.actionsVisibleKeys.insert(key) } else { self.actionsVisibleKeys.remove(key) }
          self.rebuildRows()
        }, onExpansionChange: { [weak self] key, expanded, toolKeys in
          guard let self else { return }
          if expanded { self.thinkingExpandedKeys.insert(key) } else { self.thinkingExpandedKeys.remove(key) }
          self.expandedToolKeys[key] = toolKeys
          self.rebuildRows()
        })
    }
  }
}
