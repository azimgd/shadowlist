import ShadowListUtils
import UIKit

/* A real consumer of the exported components; all application state stays on this screen. */
final class ComponentsScreen: UIViewController {
  private var layoutWidth: CGFloat = -1
  private let scroll = UIScrollView()
  private let stack = UIStackView()
  private let status = UILabel()
  private let thinking = ShadowListUtilsAssistantThinking(thinking: "Compare the travel dates, available seats and the total fare before suggesting a route.", active: true, theme: AppTheme.theme)
  private let tool = ShadowListUtilsAssistantToolCallCard(call: ShadowListUtilsAssistantToolCall(key: "search", name: "Search fares", input: "Lisbon, three days", status: .running), theme: AppTheme.theme)
  private let typing = ShadowListUtilsAssistantTypingIndicator(theme: AppTheme.theme)
  private let footer = ShadowListUtilsPollFooter(totalVotes: 3, theme: AppTheme.theme)
  private let latest = ShadowListUtilsAssistantScrollButton(theme: AppTheme.theme)

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Components"
    view.backgroundColor = AppTheme.colors.background
    scroll.translatesAutoresizingMaskIntoConstraints = false
    stack.translatesAutoresizingMaskIntoConstraints = false
    stack.axis = .vertical; stack.spacing = AppTheme.theme.spacing.lg
    view.addSubview(scroll); scroll.addSubview(stack)
    NSLayoutConstraint.activate([
      scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor), scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor), scroll.bottomAnchor.constraint(equalTo: view.keyboardLayoutGuide.topAnchor),
      stack.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor), stack.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: 16),
      stack.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -16), stack.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -24),
      stack.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -32),
    ])
    status.text = "Choose a suggestion"; status.numberOfLines = 0; status.textColor = AppTheme.colors.label
    stack.addArrangedSubview(status)
    let empty = ShadowListUtilsAssistantEmpty(theme: AppTheme.theme)
    empty.suggestions = [ShadowListUtilsAssistantSuggestion(title: "Plan a trip", prompt: "Three days in Lisbon"), ShadowListUtilsAssistantSuggestion(title: "Compare fares", prompt: "Find a quieter travel day")]
    empty.onSelectSuggestion = { [weak self] item in self?.status.text = "Selected: \(item.prompt)" }
    stack.addArrangedSubview(empty)
    stack.addArrangedSubview(thinking)
    stack.addArrangedSubview(tool)
    stack.addArrangedSubview(typing)
    stack.addArrangedSubview(footer)
    let finish = UIButton(type: .system)
    finish.setTitle("Finish search and add a vote", for: .normal)
    finish.addAction(UIAction { [weak self] _ in self?.finish() }, for: .touchUpInside)
    stack.addArrangedSubview(finish)
    latest.visible = true
    latest.onPress = { [weak self] in self?.scroll.setContentOffset(.zero, animated: true); self?.status.text = "Returned to the first component" }
    stack.addArrangedSubview(latest)
    latest.heightAnchor.constraint(equalToConstant: 36).isActive = true
    let accountValue = ShadowListUtilsFormValue(text: "3 saved routes", theme: AppTheme.theme)
    let save = ShadowListUtilsFormButton(label: "Save route", theme: AppTheme.theme)
    save.onPress = { [weak self, weak accountValue] in
      accountValue?.text = "4 saved routes"
      self?.status.text = "Route saved through Form.Button"
    }
    let delete = ShadowListUtilsFormButton(label: "Remove saved route", theme: AppTheme.theme)
    delete.destructive = true
    delete.onPress = { [weak self, weak accountValue] in
      accountValue?.text = "3 saved routes"
      self?.status.text = "Saved route removed"
    }
    stack.addArrangedSubview(ShadowListUtilsGroupedCaption(title: "Saved routes", theme: AppTheme.theme))
    stack.addArrangedSubview(ShadowListUtilsFormCard(children: [save, delete], theme: AppTheme.theme))
    let separator = ShadowListUtilsGroupedSeparator(theme: AppTheme.theme)
    let grouped = ShadowListUtilsGroupedCard(children: [accountValue, separator], theme: AppTheme.theme)
    stack.addArrangedSubview(grouped)
    stack.addArrangedSubview(ShadowListUtilsGroupedSection(title: "Travel account", children: [ShadowListUtilsFormValue(text: "Lisbon · Tuesday", theme: AppTheme.theme)], theme: AppTheme.theme))
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    // Width-dependent native text views expose their new intrinsic height to the stack.
    if stack.bounds.width != layoutWidth {
      layoutWidth = stack.bounds.width
      for child in stack.arrangedSubviews { child.invalidateIntrinsicContentSize() }
    }
  }

  private func finish() {
    thinking.active = false; thinking.thinkingMs = 1800
    tool.call = ShadowListUtilsAssistantToolCall(key: "search", name: "Search fares", input: "Lisbon, three days", status: .done(output: "Three routes are available. The quieter option leaves on Tuesday."))
    typing.isHidden = true
    footer.totalVotes += 1
    status.text = "Search complete; vote total \(Int(footer.totalVotes))"
  }
}
