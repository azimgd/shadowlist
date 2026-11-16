import ShadowListUtils
import ShadowListKit
import UIKit

/*
 * A real consumer of the exported components; all application state stays on this screen.
 */
final class TemplatesScreen: UIViewController {
  private let theme = AppTheme.theme
  private var layoutWidth: CGFloat = -1
  private let scroll = UIScrollView()
  private let stack = UIStackView()
  private let status = UILabel()
  private lazy var thinking = ShadowListUtilsAssistantThinking(thinking: "Compare the travel dates, available seats and the total fare before suggesting a route.", active: true, theme: theme)
  private lazy var tool = ShadowListUtilsAssistantToolCallCard(call: ShadowListUtilsAssistantToolCall(key: "search", name: "Search fares", input: "Lisbon, three days", status: .running), theme: theme)
  private lazy var typing = ShadowListUtilsAssistantTypingIndicator(theme: theme)
  private lazy var footer = ShadowListUtilsPollFooter(totalVotes: 3, theme: theme)
  private lazy var latest = ShadowListUtilsAssistantScrollButton(theme: theme)

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Templates"
    view.backgroundColor = theme.colors.groupedBackground
    if #available(iOS 17, *) { view.keyboardLayoutGuide.usesBottomSafeArea = false }
    scroll.translatesAutoresizingMaskIntoConstraints = false
    stack.translatesAutoresizingMaskIntoConstraints = false
    stack.axis = .vertical; stack.spacing = theme.spacing.lg
    view.addSubview(scroll); scroll.addSubview(stack)
    NSLayoutConstraint.activate([
      scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor), scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor), scroll.bottomAnchor.constraint(equalTo: view.keyboardLayoutGuide.topAnchor),
      stack.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: theme.spacing.xxl), stack.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: theme.spacing.lg),
      stack.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -theme.spacing.lg), stack.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -theme.spacing.xxl),
      stack.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -2 * theme.spacing.lg),
    ])
    stack.addArrangedSubview(TemplatesCatalog())
    stack.addArrangedSubview(ShadowListUtilsGroupedCaption(title: "Assistant and advanced components", theme: theme))
    status.text = "Choose a suggestion"; status.numberOfLines = 0; status.textColor = theme.colors.label
    stack.addArrangedSubview(status)
    let empty = ShadowListUtilsAssistantEmpty(theme: theme)
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
    let accountValue = ShadowListUtilsFormValue(text: "3 saved routes", theme: theme)
    let save = ShadowListUtilsFormButton(label: "Save route", theme: theme)
    save.onPress = { [weak self, weak accountValue] in
      accountValue?.text = "4 saved routes"
      self?.status.text = "Route saved through Form.Button"
    }
    let delete = ShadowListUtilsFormButton(label: "Remove saved route", theme: theme)
    delete.destructive = true
    delete.onPress = { [weak self, weak accountValue] in
      accountValue?.text = "3 saved routes"
      self?.status.text = "Saved route removed"
    }
    stack.addArrangedSubview(ShadowListUtilsGroupedCaption(title: "Saved routes", theme: theme))
    stack.addArrangedSubview(ShadowListUtilsFormCard(children: [save, delete], theme: theme))
    let separator = ShadowListUtilsGroupedSeparator(theme: theme)
    let grouped = ShadowListUtilsGroupedCard(children: [accountValue, separator], theme: theme)
    stack.addArrangedSubview(grouped)
    stack.addArrangedSubview(ShadowListUtilsGroupedSection(title: "Travel account", children: [ShadowListUtilsFormValue(text: "Lisbon · Tuesday", theme: theme)], theme: theme))
    let routeName = ShadowListUtilsFormInput(value: "Lisbon", accessibilityLabel: "Route name", theme: theme)
    routeName.placeholder = "Route name"
    routeName.returnKeyType = .done
    routeName.onChange = { [weak routeName] in routeName?.value = $0 }
    routeName.onSubmitEditing = { [weak self, weak routeName] in self?.status.text = "Route submitted: \(routeName?.value ?? "")" }
    let budget = ShadowListUtilsFormInput(value: "24", accessibilityLabel: "Budget", theme: theme)
    budget.numeric = true
    budget.onChange = { [weak budget] in budget?.value = $0 }
    stack.addArrangedSubview(ShadowListUtilsFormCard(children: [
      ShadowListUtilsFormRow(label: "Route", children: [routeName], theme: theme),
      ShadowListUtilsFormRow(label: "Budget", children: [budget], theme: theme),
    ], theme: theme))
    let search = ShadowListUtilsSearchField(value: "", theme: theme)
    search.labels.placeholder = "Filter routes"
    search.onChange = { [weak self, weak search] in search?.value = $0; self?.status.text = "Filter: \($0)" }
    stack.addArrangedSubview(search)
    let notes = ShadowListUtilsTextField(value: "", theme: theme)
    notes.placeholder = "Travel notes"; notes.accessibilityLabel = "Travel notes"; notes.multiline = true
    notes.onChange = { [weak notes] in notes?.value = $0 }
    stack.addArrangedSubview(notes)
    var rippleTheme = theme
    rippleTheme.pressFeedback = .ripple
    let badge = ShadowListUtilsIndexBadge(index: 1, theme: theme)
    let proceed = ShadowListUtilsPillButton(label: "Continue", theme: rippleTheme)
    proceed.onPress = { [weak self, weak proceed] in
      proceed?.disabled = true
      self?.status.text = "Route ready; add another route to continue again"
    }
    let add = ShadowListUtilsDashedButton(label: "Add route", theme: theme)
    add.onPress = { [weak self, weak badge, weak proceed] in
      guard let badge else { return }
      badge.index += 1; proceed?.disabled = false
      self?.status.text = "Added route \(badge.index)"
    }
    let glyph = UILabel()
    glyph.isAccessibilityElement = false; glyph.text = "⋯"; glyph.font = .systemFont(ofSize: 22); glyph.textColor = theme.colors.accent
    let more = ShadowListUtilsIconButton(icon: glyph, accessibilityLabel: "More routes", theme: rippleTheme)
    more.onPress = { [weak self] in self?.status.text = "More routes requested" }
    let spacer = UIView()
    spacer.setContentHuggingPriority(.init(1), for: .horizontal)
    proceed.setContentHuggingPriority(.required, for: .horizontal)
    let buttons = UIStackView(arrangedSubviews: [badge, proceed, more, spacer])
    buttons.axis = .horizontal; buttons.alignment = .center; buttons.spacing = theme.spacing.md
    NSLayoutConstraint.activate([badge.widthAnchor.constraint(equalToConstant: 22), badge.heightAnchor.constraint(equalToConstant: 22), more.widthAnchor.constraint(equalToConstant: 40), more.heightAnchor.constraint(equalToConstant: 40)])
    stack.addArrangedSubview(buttons); stack.addArrangedSubview(add)
    let direct = ShadowListUtilsChip(label: "Direct routes", theme: theme)
    direct.selected = true; direct.maxFontSizeMultiplier = 1.5
    let seat = ShadowListUtilsChip(label: "Seat 12A", theme: theme)
    seat.variant = .plain; seat.size = .compact; seat.mono = true
    let train = ShadowListUtilsChoiceChip(label: "Train", tint: theme.colors.fill, selected: true, theme: theme)
    let flight = ShadowListUtilsChoiceChip(label: "Flight", tint: theme.colors.accentSoft, selected: false, theme: theme)
    let chips = ShadowListUtilsChipRow(children: [direct, seat, train, flight], theme: theme)
    direct.onPress = { [weak self, weak direct] in
      guard let direct else { return }
      direct.selected.toggle(); self?.status.text = direct.selected ? "Direct routes selected" : "Direct routes cleared"
    }
    direct.onClear = { [weak self, weak chips, weak direct] in
      chips?.children.removeAll { $0 === direct }; self?.status.text = "Direct routes removed"
    }
    seat.onClear = { [weak self, weak chips, weak seat] in
      chips?.children.removeAll { $0 === seat }; self?.status.text = "Seat preference removed"
    }
    train.onPress = { [weak self, weak train, weak flight] in
      train?.selected = true; flight?.selected = false; self?.status.text = "Train selected"
    }
    flight.onPress = { [weak self, weak train, weak flight] in
      train?.selected = false; flight?.selected = true; self?.status.text = "Flight selected"
    }
    stack.addArrangedSubview(chips)
    let segmented = ShadowListUtilsSegmented(options: [
      ShadowListUtilsSegmentedOption(id: "plan", label: "Plan"),
      ShadowListUtilsSegmentedOption(id: "book", label: "Book"),
      ShadowListUtilsSegmentedOption(id: "travel", label: "Travel"),
    ], value: "plan", theme: theme)
    segmented.accessibilityLabel = "Trip stage"
    let bar = ShadowListUtilsProgressBar(fraction: 0.285, accessibilityLabel: "Trip preparation progress", theme: theme)
    let ring = ShadowListUtilsProgressRing(fraction: 0.285, theme: theme)
    var requests = 0
    segmented.onChange = { [weak self, weak segmented, weak bar, weak ring] id in
      guard let segmented, let bar else { return }
      requests += 1; segmented.value = id
      bar.fraction = id == "plan" ? 0.285 : id == "book" ? 0.5 : 1
      ring?.fraction = bar.fraction
      let label = segmented.options.first(where: { $0.id == id })?.label ?? id
      self?.status.text = "\(label) selected (\(requests) requests)"
    }
    stack.addArrangedSubview(ShadowListUtilsGroupedCaption(title: "Trip preparation", theme: theme))
    stack.addArrangedSubview(segmented); stack.addArrangedSubview(bar)
    let ringContainer = UIView()
    ring.translatesAutoresizingMaskIntoConstraints = false; ringContainer.addSubview(ring)
    NSLayoutConstraint.activate([ring.widthAnchor.constraint(equalToConstant: 52), ring.heightAnchor.constraint(equalToConstant: 52), ring.centerXAnchor.constraint(equalTo: ringContainer.centerXAnchor), ring.topAnchor.constraint(equalTo: ringContainer.topAnchor), ring.bottomAnchor.constraint(equalTo: ringContainer.bottomAnchor)])
    stack.addArrangedSubview(ringContainer)
    appendGroupedActions()
    latest.visible = true
    latest.onPress = { [weak self] in self?.scroll.setContentOffset(.zero, animated: true); self?.status.text = "Returned to the first component" }
    let latestContainer = UIView()
    latest.translatesAutoresizingMaskIntoConstraints = false
    latestContainer.addSubview(latest)
    NSLayoutConstraint.activate([latest.widthAnchor.constraint(equalToConstant: 36), latest.heightAnchor.constraint(equalToConstant: 36), latest.centerXAnchor.constraint(equalTo: latestContainer.centerXAnchor), latest.topAnchor.constraint(equalTo: latestContainer.topAnchor), latest.bottomAnchor.constraint(equalTo: latestContainer.bottomAnchor)])
    stack.addArrangedSubview(latestContainer)
  }

  private func appendGroupedActions() {
    stack.addArrangedSubview(ShadowListUtilsGroupedCaption(title: "Reservation actions", theme: theme))
    let row = ShadowListUtilsGroupedRow(title: "Reservation", theme: theme)
    row.subtitle = "Kyoto, Tuesday"; row.accessibilityRole = .link; row.testID = "reservation-row"
    let map = UIButton(type: .system); map.setTitle("Map", for: .normal)
    map.addAction(UIAction { [weak self] _ in self?.status.text = "Map opened" }, for: .touchUpInside)
    row.leading = map
    let nights = UILabel(); nights.text = "2 nights"; nights.textColor = theme.colors.secondaryLabel
    row.trailing = nights
    row.onPress = { [weak self] in self?.status.text = "Reservation opened" }
    row.onLongPress = { [weak self] in self?.status.text = "Reservation details" }; row.longPressLabel = "Reservation details"
    row.trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(actions: [
      ShadowListKitSwipeAction(style: .destructive, title: "Remove") { [weak self, weak row] _, done in
        row?.title = "Reservation removed"; self?.status.text = "Reservation removed"; done(true)
      },
    ])
    row.trailingSwipeActions?.performsFirstActionWithFullSwipe = false
    row.contextMenu = UIMenu(children: [UIAction(title: "Inspect reservation") { [weak self] _ in self?.status.text = "Reservation menu inspected" }])
    let disabled = ShadowListUtilsGroupedRow(title: "Read-only reservation", theme: theme)
    disabled.subtitle = "Actions remain available"; disabled.disabled = true; disabled.separated = true
    disabled.onPress = { [weak self] in self?.status.text = "Unexpected disabled primary press" }
    disabled.leadingSwipeActions = ShadowListKitSwipeActionsConfiguration(actions: [
      ShadowListKitSwipeAction(style: .normal, title: "Save") { [weak self] _, done in self?.status.text = "Read-only reservation saved"; done(true) },
    ])
    disabled.contextMenu = UIMenu(children: [UIAction(title: "Inspect read-only reservation") { [weak self] _ in self?.status.text = "Read-only menu inspected" }])
    stack.addArrangedSubview(ShadowListUtilsGroupedCard(children: [row, disabled], theme: theme))
    let booking = UIButton(type: .system); booking.setTitle("Swipe this booking", for: .normal)
    booking.backgroundColor = theme.colors.groupedCell
    booking.heightAnchor.constraint(greaterThanOrEqualToConstant: 44).isActive = true
    booking.addAction(UIAction { [weak self] _ in self?.status.text = "Booking opened" }, for: .touchUpInside)
    let standalone = ShadowListUtilsGroupedActions(children: [booking], theme: theme)
    standalone.trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(actions: [
      ShadowListKitSwipeAction(style: .normal, title: "Save booking") { [weak self] _, done in self?.status.text = "Booking saved"; done(true) },
    ])
    standalone.contextMenu = UIMenu(children: [UIAction(title: "Inspect booking") { [weak self] _ in self?.status.text = "Booking menu inspected" }])
    stack.addArrangedSubview(standalone)
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
