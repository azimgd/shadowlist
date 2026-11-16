import ShadowListKit
import ShadowListUtils
import UIKit

/*
 * A row of the lab: a title and one to four lines of filler. Every seventh or fifth row is
 * marked. In the spans layout the filler is not cut to its lines.
 */
struct LabRow: ShadowListUtilsRow {
  let index: Int
  let lines: Int
  let marked: Bool
  let truncates: Bool

  init(index: Int, marked: Bool, truncates: Bool) {
    self.index = index
    lines = 1 + (index * 7) % 4
    self.marked = marked
    self.truncates = truncates
  }

  var key: String { "lab-\(index)" }
  var contentVersion: Int { (marked ? 1 : 0) | (truncates ? 2 : 0) }
  var viewClass: ShadowListUtilsRowView.Type { LabRowView.self }

  func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = AppTheme.theme.spacing
    let textWidth = max(1, width - 2 * spacing.lg)
    let title = AppTheme.typography.headline.layout("Row \(index)", width: textWidth)
    let text = AppTheme.typography.subhead.layout(
      Array(repeating: Self.filler, count: lines).joined(separator: " "), width: textWidth, maxLines: truncates ? lines : 0)
    let titleFrame = CGRect(origin: CGPoint(x: spacing.lg, y: spacing.md), size: title.size)
    let textFrame = CGRect(origin: CGPoint(x: spacing.lg, y: titleFrame.maxY), size: text.size)
    return LabRowLayout(width: width, height: textFrame.maxY + spacing.md, title: title, titleFrame: titleFrame, text: text, textFrame: textFrame)
  }

  private static let filler = "Rows have different heights so scroll commands land through estimated sizes."
}

final class LabRowLayout: ShadowListUtilsRowLayout {
  let title: ShadowListKitTextLayout
  let titleFrame: CGRect
  let text: ShadowListKitTextLayout
  let textFrame: CGRect

  init(width: CGFloat, height: CGFloat, title: ShadowListKitTextLayout, titleFrame: CGRect, text: ShadowListKitTextLayout, textFrame: CGRect) {
    self.title = title
    self.titleFrame = titleFrame
    self.text = text
    self.textFrame = textFrame
    super.init(width: width, height: height)
  }
}

final class LabRowView: ShadowListUtilsRowView {
  private let title = ShadowListKitTextView(frame: .zero)
  private let text = ShadowListKitTextView(frame: .zero)
  private var marked = false

  required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    accessibilityTraits = .button
    layer.cornerRadius = AppTheme.theme.radius.sm
    title.textColor = AppTheme.colors.label
    text.textColor = AppTheme.colors.secondaryLabel
    addSubview(title)
    addSubview(text)
  }

  override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? LabRow else { return }
    marked = row.marked
    accessibilityLabel = "Row \(row.index)"
    showBackground()
  }

  override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let layout = layout as? LabRowLayout else { return }
    title.textLayout = layout.title
    title.frame = layout.titleFrame
    text.textLayout = layout.text
    text.frame = layout.textFrame
  }

  override func setHighlighted(_ highlighted: Bool, animated: Bool) {
    super.setHighlighted(highlighted, animated: animated)
    showBackground()
  }

  private func showBackground() {
    backgroundColor = isHighlighted ? AppTheme.colors.fill : marked ? AppTheme.colors.accentSoft : AppTheme.colors.background
  }
}

/*
 * The list's scroll commands, scroll events and viewability in one place. It opens at row 120.
 * The More menu jumps, saves and restores the position and switches layouts. Two viewability
 * configs report at once.
 */
final class ScrollLabScreen: PageScreen, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  private let list = ShadowListKitListView()
  private let layouts = ShadowListUtilsLayoutCache()
  private var rows: [LabRow] = []
  private let status = UILabel()
  private let header = UIView()
  private let layout = ShadowListUtilsSegmented(options: [
    ShadowListUtilsSegmentedOption(id: "1", label: "List"),
    ShadowListUtilsSegmentedOption(id: "2", label: "Grid"),
    ShadowListUtilsSegmentedOption(id: "3", label: "Spans"),
  ], value: "1", theme: AppTheme.theme)
  private let padding = ShadowListUtilsSegmented(options: [
    ShadowListUtilsSegmentedOption(id: "none", label: "No Padding"),
    ShadowListUtilsSegmentedOption(id: "padded", label: "Padding"),
  ], value: "none", theme: AppTheme.theme)
  private let query = ShadowListUtilsTextField(value: "", theme: AppTheme.theme)

  private var columns = 1
  private var marked = 7
  private var anchor: ShadowListKitAnchorState?
  private var positioned = false
  private var momentumEndCount = 0

  private var scrollLine = "—"
  private var phaseLine = "—"
  private var viewableLine = "—"
  private var coveredLine = "—"
  private var commandLine = "—"
  private var pressedLine = "—"

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Scroll Lab"
    list.backgroundColor = AppTheme.colors.background
    list.contentInsetAdjustmentBehavior = .never
    list.keyboardDismissMode = .onDrag
    list.estimatedItemSize = 120
    list.separatorInsetStart = AppTheme.theme.rowInset
    list.separatorColor = AppTheme.colors.separator
    list.register(LabRowView.self, forCellReuseIdentifier: "lab")
    list.viewabilityConfigCallbackPairs = viewabilityPairs()
    list.dataSource = self
    list.delegate = self
    installHeader()
    page.addSubview(list)
    if Settings.debug { installStatus() }
    applyLayout()
    installActions()
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    let statusHeight = status.superview == nil ? 0 : Self.statusLines * AppTheme.typography.caption.lineHeight + 2 * AppTheme.theme.spacing.xs
    status.superview?.frame = CGRect(x: 0, y: 0, width: page.bounds.width, height: statusHeight)
    status.frame = (status.superview?.bounds ?? .zero).insetBy(dx: AppTheme.theme.spacing.lg, dy: AppTheme.theme.spacing.xs)
    list.frame = CGRect(x: 0, y: statusHeight, width: page.bounds.width, height: page.bounds.height - statusHeight)
    layoutHeader()
    openAtInitialIndex()
  }

  // MARK: - Header

  private func installHeader() {
    header.backgroundColor = AppTheme.colors.groupedBackground
    layout.size = .compact
    layout.accessibilityLabel = "Layout"
    layout.onChange = { [weak self] id in
      guard let self else { return }
      self.layout.value = id
      self.columns = Int(id) ?? 1
      self.applyLayout()
    }
    padding.size = .compact
    padding.accessibilityLabel = "Content padding"
    padding.onChange = { [weak self] id in
      guard let self else { return }
      self.padding.value = id
      self.list.contentInset = id == "padded" ? Self.paddedContent : .zero
    }
    query.placeholder = "Type, then tap a row"
    query.onChange = { [weak self] in self?.query.value = $0 }
    [layout, padding, query].forEach(header.addSubview)
    list.headerView = header
  }

  private func layoutHeader() {
    let spacing = AppTheme.theme.spacing
    let width = list.bounds.width - list.contentInset.left - list.contentInset.right
    let inner = CGSize(width: width - 2 * spacing.lg, height: .greatestFiniteMagnitude)
    var y = spacing.lg
    for (index, view) in [layout, padding, query].enumerated() {
      if index > 0 { y += spacing.sm }
      let height = view.sizeThatFits(inner).height
      view.frame = CGRect(x: spacing.lg, y: y, width: inner.width, height: height)
      y += height
    }
    let size = CGSize(width: width, height: y + spacing.lg)
    if header.frame.size != size {
      header.frame.size = size
      list.headerView = header
    }
  }

  // MARK: - Status

  private func installStatus() {
    let container = UIView()
    container.backgroundColor = AppTheme.colors.groupedBackground
    container.accessibilityIdentifier = "lab-status"
    status.numberOfLines = Int(Self.statusLines)
    status.lineBreakMode = .byClipping
    container.addSubview(status)
    page.addSubview(container)
    showStatus()
  }

  private func showStatus() {
    guard status.superview != nil else { return }
    let size = list.contentSize
    let lines = [
      "scroll \(scrollLine)",
      "phase \(phaseLine) size \(Int(size.width.rounded()))x\(Int(size.height.rounded()))",
      "viewable \(viewableLine) covered \(coveredLine)",
      "command \(commandLine) pressed \(pressedLine)",
    ]
    status.attributedText = AppTheme.typography.caption.string(lines.joined(separator: "\n"), AppTheme.colors.secondaryLabel)
  }

  private func describeScroll(_ name: String) -> String {
    "\(name) y=\(Int(list.contentOffset.y.rounded()))/\(Int(list.contentSize.height.rounded())) h=\(Int(list.bounds.height.rounded()))"
  }

  // MARK: - Layout

  /*
   * The rows for the layout and the marked rows. A new layout opens at the initial row again.
   */
  private func applyLayout() {
    let reopens = list.numberOfColumns != columns
    list.numberOfColumns = columns
    list.showsSeparators = columns == 1
    list.getItemSpan = columns == 3 ? { [weak self] index in
      index % 17 == 0 ? 3 : index % (self?.marked ?? 7) == 0 ? 2 : 1
    } : nil
    rows = (0..<Self.rowCount).map { LabRow(index: $0, marked: $0 % marked == 0, truncates: columns != 3) }
    if reopens || !positioned {
      list.reloadData()
      positioned = false
      view.setNeedsLayout()
    } else {
      list.applyChanges()
    }
  }

  private func openAtInitialIndex() {
    guard !positioned, list.bounds.height > 0 else { return }
    positioned = true
    list.scrollToItem(at: Self.initialIndex, viewPosition: 0, animated: false)
  }

  // MARK: - Actions

  private func installActions() {
    func action(_ title: String, _ symbol: String, _ work: @escaping (ScrollLabScreen) -> Void) -> UIAction {
      UIAction(title: title, image: UIImage(systemName: symbol)) { [weak self] _ in
        guard let self else { return }
        work(self)
        self.showStatus()
      }
    }
    let scrolling = action("Scrolling Enabled", "hand.draw") {
      $0.list.isScrollEnabled.toggle()
      $0.installActions()
    }
    scrolling.state = list.isScrollEnabled ? .on : .off
    headerMenu([
      [
        action("Scroll to Row 300 (Top)", "arrow.up.to.line") { $0.scrollTo(300, viewPosition: 0) },
        action("Scroll to Row 40 (Center)", "arrow.up.and.down") { $0.scrollTo(40, viewPosition: 0.5) },
        action("Scroll to Row 450 (Bottom, 20 pt)", "arrow.down.to.line") { $0.scrollTo(450, viewPosition: 1, viewOffset: 20) },
        action("Scroll to Row 9999", "exclamationmark.triangle") { $0.scrollTo(9999, viewPosition: 0) },
      ],
      [
        action("Scroll to Start", "arrow.up") {
          $0.commandLine = "offset 0"
          $0.list.scrollToStart(animated: true)
        },
        action("Scroll to End", "arrow.down") {
          $0.commandLine = "end"
          $0.list.scrollToEnd(animated: true)
        },
        action("Flash Scroll Indicators", "sparkles") {
          $0.list.flashScrollIndicators()
          $0.commandLine = "flash"
        },
      ],
      [
        action("Save Position", "bookmark") {
          $0.anchor = $0.list.anchorState
          $0.commandLine = $0.anchor.map { "saved \($0.key)@\(String(format: "%.1f", $0.offset))" } ?? "saved none"
        },
        action("Restore Position", "bookmark.fill") {
          guard let anchor = $0.anchor else { return }
          $0.commandLine = "restore \(anchor.key)@\(String(format: "%.1f", anchor.offset))"
          $0.list.restoreAnchorState(anchor)
        },
      ],
      [
        scrolling,
        action("Mark Every \(marked == 7 ? 5 : 7)th Row", "number") {
          $0.marked = $0.marked == 7 ? 5 : 7
          $0.applyLayout()
          $0.installActions()
        },
      ],
    ])
  }

  /*
   * Scroll to a row. The kit places a row by viewPosition alone, and viewOffset moves the list
   * on from there once it rests. A row the data lacks reports the failure and does nothing.
   */
  private func scrollTo(_ index: Int, viewPosition: CGFloat, viewOffset: CGFloat = 0) {
    guard rows.indices.contains(index) else {
      commandLine = "failed \(index) count=\(rows.count)"
      return
    }
    commandLine = "to \(index) pos=\(viewPosition) off=\(Int(viewOffset))"
    pendingViewOffset = viewOffset
    list.scrollToItem(at: index, viewPosition: viewPosition, animated: true)
  }

  private var pendingViewOffset: CGFloat = 0

  // MARK: - Viewability

  /*
   * Two viewability configs at once: rows at least half visible, and rows covering a tenth of
   * the viewport for 250 ms after the first interaction.
   */
  private func viewabilityPairs() -> [ShadowListKitViewabilityConfigCallbackPair] {
    func range(_ info: ShadowListKitViewableItemsChangedInfo) -> String? {
      let indices = info.viewableItems.map(\.index)
      guard let low = indices.min(), let high = indices.max() else { return nil }
      return "\(low)–\(high)"
    }
    return [
      ShadowListKitViewabilityConfigCallbackPair(
        viewabilityConfig: ShadowListKitViewabilityConfig(
          itemVisiblePercentThreshold: 50, viewAreaCoveragePercentThreshold: nil, minimumViewTime: 0, waitForInteraction: false)
      ) { [weak self] info in
        self?.viewableLine = range(info).map { "\($0) (+\(info.changed.filter(\.isViewable).count))" } ?? "—"
        self?.showStatus()
      },
      ShadowListKitViewabilityConfigCallbackPair(
        viewabilityConfig: ShadowListKitViewabilityConfig(
          itemVisiblePercentThreshold: nil, viewAreaCoveragePercentThreshold: 10, minimumViewTime: 250, waitForInteraction: true)
      ) { [weak self] info in
        self?.coveredLine = range(info) ?? "—"
        self?.showStatus()
      },
    ]
  }

  // MARK: - Data

  func numberOfItems(in listView: ShadowListKitListView) -> Int { rows.count }
  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String { rows[index].key }
  func listView(_ listView: ShadowListKitListView, contentVersionForItemAt index: Int) -> Int { rows[index].contentVersion }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    let cell = listView.dequeueReusableCell(withIdentifier: "lab")
    (cell as? LabRowView)?.configure(rows[index], layout: nil)
    return cell
  }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    layouts.layout(rows[index], width: crossSize).height + (listView.showsSeparators ? ShadowListUtilsTheme.hairline : 0)
  }

  // MARK: - Events

  func listView(_ listView: ShadowListKitListView, didSelectItemAt index: Int) {
    listView.deselectItem(at: index, animated: false)
    pressedLine = "row \(index)"
    showStatus()
  }

  func listViewDidScroll(_ listView: ShadowListKitListView) {
    scrollLine = describeScroll("")
    showStatus()
  }

  func listViewWillBeginDragging(_ listView: ShadowListKitListView) {
    pendingViewOffset = 0
    phaseLine = describeScroll("drag")
    showStatus()
  }

  func listViewDidEndDragging(_ listView: ShadowListKitListView, willDecelerate decelerate: Bool) {
    phaseLine = describeScroll(decelerate ? "glide" : "release")
    showStatus()
  }

  func listViewDidEndDecelerating(_ listView: ShadowListKitListView) {
    momentumEndCount += 1
    phaseLine = "\(describeScroll("rest")) end=\(momentumEndCount)"
    showStatus()
  }

  func listViewDidEndScrollingAnimation(_ listView: ShadowListKitListView) {
    guard pendingViewOffset != 0 else { return }
    let offset = pendingViewOffset
    pendingViewOffset = 0
    listView.setContentOffset(CGPoint(x: listView.contentOffset.x, y: listView.contentOffset.y - offset), animated: true)
  }

  private static let rowCount = 500
  private static let initialIndex = 120
  private static let statusLines: CGFloat = 4
  private static let paddedContent = UIEdgeInsets(top: 24, left: 16, bottom: 48, right: 16)
}
