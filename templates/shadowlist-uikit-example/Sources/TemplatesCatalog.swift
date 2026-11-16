import ShadowListUtils
import UIKit

/*
 * Interactive controls at the top of the catalog mirror the React Native showcase.
 */
final class TemplatesCatalog: UIStackView {
  private let theme = AppTheme.theme
  private var choices: [ShadowListUtilsChoiceChip] = []

  init() {
    super.init(frame: .zero)
    axis = .vertical
    spacing = theme.spacing.xxl
    let save = pill("Save", .primary)
    let undo = pill("Undo", .tinted)
    let cancel = pill("Cancel", .plain)
    let delete = pill("Delete", .destructive)
    let disabled = pill("Disabled", .primary)
    disabled.disabled = true
    let plus = UIImageView(image: ShadowListUtilsPlusIcon.image())
    plus.tintColor = theme.colors.accent
    plus.contentMode = .center
    let add = ShadowListUtilsIconButton(icon: plus, accessibilityLabel: "Add", theme: theme)
    add.onPress = {}
    add.widthAnchor.constraint(equalToConstant: 40).isActive = true
    add.heightAnchor.constraint(equalToConstant: 40).isActive = true
    let sort = ShadowListUtilsDashedButton(label: "Add sort", theme: theme)
    sort.onPress = {}
    section("Buttons", [line([save, undo, cancel, delete]), line([disabled, add]), sort])

    let fill = ShadowListUtilsChip(label: "Fill down", theme: theme)
    fill.onPress = { [weak fill] in fill?.selected.toggle() }
    let clear = ShadowListUtilsChip(label: "Clear", theme: theme)
    clear.variant = .plain
    clear.onPress = { [weak fill] in fill?.selected = false }
    choices = ["Planned", "Booked", "Done"].enumerated().map { index, title in
      ShadowListUtilsChoiceChip(label: title, tint: [theme.colors.accentSoft, theme.colors.redSoft, theme.colors.fill][index], selected: index == 0, theme: theme)
    }
    for choice in choices {
      choice.onPress = { [weak self, weak choice] in
        guard let self, let choice else { return }
        for item in self.choices { item.selected = item === choice }
      }
    }
    section("Chips", [ShadowListUtilsChipRow(children: [fill, clear], theme: theme), ShadowListUtilsChipRow(children: choices, theme: theme)])

    let search = ShadowListUtilsSearchField(value: "", theme: theme)
    search.onChange = { [weak search] in search?.value = $0 }
    let code = ShadowListUtilsTextField(value: "", theme: theme)
    code.placeholder = "Gate code"
    code.onChange = { [weak code] in code?.value = $0 }
    let notes = ShadowListUtilsTextField(value: "", theme: theme)
    notes.placeholder = "Paste rows here"
    notes.multiline = true
    notes.onChange = { [weak notes] in notes?.value = $0 }
    section("Fields", [search, code, notes])

    let periods = ShadowListUtilsSegmented(options: [
      ShadowListUtilsSegmentedOption(id: "day", label: "Day"),
      ShadowListUtilsSegmentedOption(id: "week", label: "Week"),
      ShadowListUtilsSegmentedOption(id: "month", label: "Month"),
    ], value: "week", theme: theme)
    periods.onChange = { [weak periods] in periods?.value = $0 }
    let order = ShadowListUtilsSegmented(options: [
      ShadowListUtilsSegmentedOption(id: "asc", label: "A→Z"),
      ShadowListUtilsSegmentedOption(id: "desc", label: "Z→A"),
    ], value: "asc", theme: theme)
    order.size = .compact
    order.onChange = { [weak order] in order?.value = $0 }
    section("Segmented", [periods, order])

    let name = ShadowListUtilsFormInput(value: "Boarding pass", accessibilityLabel: "Name", theme: theme)
    name.onChange = { [weak name] in name?.value = $0 }
    let price = ShadowListUtilsFormInput(value: "42.50", accessibilityLabel: "Price", theme: theme)
    price.numeric = true
    let total = ShadowListUtilsFormValue(text: "51.00", theme: theme)
    price.onChange = { [weak price, weak total] value in
      price?.value = value
      total?.text = String(format: "%.2f", (Double(value) ?? 0) * 1.2)
    }
    let period = ShadowListUtilsSegmented(options: periods.options, value: periods.value, theme: theme)
    period.onChange = { [weak period, weak periods] in period?.value = $0; periods?.value = $0 }
    let periodRow = ShadowListUtilsFormRow(label: "Period", children: [period], theme: theme)
    periodRow.stacked = true
    let remove = ShadowListUtilsFormButton(label: "Remove item", theme: theme)
    remove.destructive = true
    remove.onPress = {}
    section("Form", [ShadowListUtilsFormCard(children: [
      ShadowListUtilsFormRow(label: "Name", children: [name], theme: theme),
      ShadowListUtilsFormRow(label: "Price", children: [price], theme: theme),
      ShadowListUtilsFormRow(label: "With tax", children: [total], theme: theme),
      periodRow,
    ], theme: theme), remove])

    let lastPress = ShadowListUtilsGroupedRow(title: "Last press: Nothing yet", theme: theme)
    lastPress.chevron = false
    let files = ShadowListUtilsGroupedRow(title: "Trip files", theme: theme)
    files.subtitle = "12 documents"
    let tile = UIImageView(image: ShadowListUtilsFolderIcon.image())
    tile.tintColor = theme.colors.onAccent
    tile.backgroundColor = theme.colors.accent
    tile.contentMode = .center
    tile.layer.cornerRadius = 7
    tile.widthAnchor.constraint(equalToConstant: 30).isActive = true
    tile.heightAnchor.constraint(equalToConstant: 30).isActive = true
    files.leading = tile
    files.onPress = { [weak lastPress] in lastPress?.title = "Last press: Trip files" }
    files.onLongPress = { [weak lastPress] in lastPress?.title = "Last press: Trip files, held" }
    files.longPressLabel = "Show details"
    let currency = ShadowListUtilsGroupedRow(title: "Currency", theme: theme)
    let value = UILabel()
    value.attributedText = theme.typography.body.string("EUR", theme.colors.secondaryLabel)
    currency.trailing = value
    currency.separated = true
    currency.onPress = { [weak lastPress] in lastPress?.title = "Last press: Currency" }
    let export = ShadowListUtilsGroupedRow(title: "Export", theme: theme)
    export.subtitle = "A long description that wraps onto a second line when it needs to"
    export.subtitleLines = 2
    export.chevron = false
    export.separated = true
    export.onPress = { [weak lastPress] in lastPress?.title = "Last press: Export" }
    let sync = ShadowListUtilsGroupedRow(title: "Sync", theme: theme)
    sync.subtitle = "Unavailable offline"
    sync.disabled = true
    sync.separated = true
    section("Grouped", [files, currency, export, sync, ShadowListUtilsGroupedSeparator(inset: 0, theme: theme), lastPress], card: true, padding: false)
    if let grouped = arrangedSubviews.last { setCustomSpacing(theme.spacing.xxl + theme.spacing.xl, after: grouped) }

    let bars = [0.25, 0.62, 0.9, 1.2].enumerated().map { index, fraction in
      let bar = ShadowListUtilsProgressBar(fraction: fraction, accessibilityLabel: ["25% saved", "62% saved", "90% spent", "Over budget"][index], theme: theme)
      bar.color = [theme.colors.accent, theme.colors.green, theme.colors.orange, theme.colors.red][index]
      if index == 3 { bar.height = 10 }
      return bar
    }
    section("Progress", bars + [line([
      ShadowListUtilsProgressRing(fraction: 0.25, theme: theme),
      ShadowListUtilsProgressRing(fraction: 0.6, theme: theme),
      ShadowListUtilsProgressRing(fraction: 0.9, size: 40, strokeWidth: 5, theme: theme),
    ])], card: true)
    section("Badges", [line((1...3).map { ShadowListUtilsIndexBadge(index: $0, theme: theme) })])
    let contentWidth = UIScreen.main.bounds.width - 2 * (theme.grouped.inset + theme.spacing.md)
    let document = UIImageView(image: ShadowListUtilsDocIcon.image(size: 28))
    document.tintColor = theme.colors.tertiaryLabel
    document.frame.size = CGSize(width: 28, height: 28)
    let empty = ShadowListUtilsEmptyState(title: "No files yet", message: "Add one below.", icon: document, theme: theme)
    empty.heightAnchor.constraint(equalToConstant: empty.sizeThatFits(CGSize(width: contentWidth, height: .greatestFiniteMagnitude)).height).isActive = true
    section("Empty state", [empty], card: true)

    let ada = ShadowListUtilsAvatar(size: 40, theme: theme)
    ada.show(name: "Ada Lovelace", imageUrl: nil)
    let grace = ShadowListUtilsAvatar(size: 32, theme: theme)
    grace.show(name: "Grace Hopper", imageUrl: nil)
    for avatar in [ada, grace] {
      avatar.widthAnchor.constraint(equalToConstant: avatar.size).isActive = true
      avatar.heightAnchor.constraint(equalToConstant: avatar.size).isActive = true
    }
    let spinner = ShadowListUtilsSpinner(theme: theme)
    let header = ShadowListUtilsListHeader(title: "List header", subtitle: "With a subtitle", theme: theme)
    header.heightAnchor.constraint(equalToConstant: header.sizeThatFits(CGSize(width: contentWidth, height: .greatestFiniteMagnitude)).height).isActive = true
    let sectionHeader = ShadowListUtilsSectionHeader(key: "section", title: "Section header", count: 3, theme: theme)
    let heading = ShadowListUtilsSectionHeaderView(reuseIdentifier: nil)
    heading.configure(sectionHeader, layout: nil)
    heading.heightAnchor.constraint(equalToConstant: sectionHeader.layout(width: contentWidth).height).isActive = true
    let footer = ShadowListUtilsListFooter(text: "List footer", theme: theme)
    footer.heightAnchor.constraint(equalToConstant: footer.sizeThatFits(CGSize(width: contentWidth, height: .greatestFiniteMagnitude)).height).isActive = true
    section("Primitives", [line([ada, grace, spinner]), header, heading, ShadowListUtilsItemSeparator(theme: theme), footer], card: true)
    section("Assistant", [line([
      ShadowListUtilsAssistantActionButton(label: "Copy", icon: ShadowListUtilsCopyIcon.image(size: 18).withTintColor(theme.colors.secondaryLabel, renderingMode: .alwaysOriginal), theme: theme, onPress: {}),
      ShadowListUtilsAssistantActionButton(label: "Retry", icon: ShadowListUtilsRetryIcon.image(size: 18), showLabel: true, theme: theme, onPress: {}),
    ])])
    let shelf = UIScrollView()
    shelf.showsHorizontalScrollIndicator = false
    shelf.heightAnchor.constraint(equalToConstant: 34).isActive = true
    let elements: [UIView] = ["Lisbon", "12", "Kyoto", "7", "Reykjavík", "Oaxaca"].enumerated().map { index, label in
      if index == 1 || index == 3 {
        let count = UILabel()
        count.attributedText = theme.typography.subhead.string(label)
        count.textAlignment = .center
        count.backgroundColor = theme.colors.elevated2
        count.layer.cornerRadius = 17
        count.clipsToBounds = true
        count.widthAnchor.constraint(greaterThanOrEqualToConstant: 34).isActive = true
        count.heightAnchor.constraint(equalToConstant: 34).isActive = true
        return count
      }
      let chip = ShadowListUtilsChip(label: label, theme: theme)
      chip.onPress = {}
      return chip
    }
    let shelfContent = UIStackView(arrangedSubviews: elements)
    shelfContent.axis = .horizontal
    shelfContent.spacing = theme.spacing.sm
    shelfContent.translatesAutoresizingMaskIntoConstraints = false
    shelf.addSubview(shelfContent)
    NSLayoutConstraint.activate([
      shelfContent.topAnchor.constraint(equalTo: shelf.contentLayoutGuide.topAnchor),
      shelfContent.bottomAnchor.constraint(equalTo: shelf.contentLayoutGuide.bottomAnchor),
      shelfContent.leadingAnchor.constraint(equalTo: shelf.contentLayoutGuide.leadingAnchor),
      shelfContent.trailingAnchor.constraint(equalTo: shelf.contentLayoutGuide.trailingAnchor),
      shelfContent.heightAnchor.constraint(equalTo: shelf.frameLayoutGuide.heightAnchor),
    ])
    section("Template list", [shelf])
  }

  required init(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  private func pill(_ label: String, _ variant: ShadowListUtilsPillButtonVariant) -> ShadowListUtilsPillButton {
    let button = ShadowListUtilsPillButton(label: label, theme: theme)
    button.variant = variant
    button.onPress = {}
    return button
  }

  private func line(_ children: [UIView]) -> UIStackView {
    let row = UIStackView(arrangedSubviews: children + [UIView()])
    row.axis = .horizontal
    row.alignment = .center
    row.spacing = theme.spacing.sm
    children.forEach { $0.setContentHuggingPriority(.required, for: .horizontal) }
    return row
  }

  private func section(_ title: String, _ children: [UIView], card: Bool = false, padding: Bool = true) {
    let caption = UILabel()
    caption.attributedText = theme.typography.footnote.string(title.uppercased(), theme.colors.secondaryLabel)
    caption.isAccessibilityElement = true
    caption.accessibilityTraits = .header
    let captionRow = UIStackView(arrangedSubviews: [caption])
    captionRow.isLayoutMarginsRelativeArrangement = true
    captionRow.layoutMargins = UIEdgeInsets(top: 0, left: theme.spacing.lg, bottom: 0, right: theme.spacing.lg)
    let body = UIStackView(arrangedSubviews: children)
    body.axis = .vertical
    body.spacing = padding ? theme.spacing.md : 0
    if card {
      body.backgroundColor = theme.colors.groupedCell
      body.layer.cornerRadius = theme.grouped.radius
      body.clipsToBounds = true
      body.isLayoutMarginsRelativeArrangement = padding
      body.layoutMargins = UIEdgeInsets(top: theme.spacing.md, left: theme.spacing.md, bottom: theme.spacing.md, right: theme.spacing.md)
    }
    let content = UIStackView(arrangedSubviews: [captionRow, body])
    content.axis = .vertical
    content.setCustomSpacing(theme.spacing.sm - 2, after: captionRow)
    if title == "Buttons", let first = children.first { body.setCustomSpacing(theme.spacing.sm, after: first) }
    addArrangedSubview(content)
  }
}
