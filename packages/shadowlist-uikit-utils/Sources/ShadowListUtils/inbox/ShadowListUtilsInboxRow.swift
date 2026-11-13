import ShadowListKit
import UIKit

/*
 * A mail row: an avatar, the sender, the message's age, the subject and a preview of two lines.
 * An unread dot and a flag mark its state. While editing it shows a selection circle in place
 * of the dot. The row highlights while pressed, and while selected in a list that is editing.
 * The age is worked out once, when the row is made.
 */
public struct ShadowListUtilsInboxRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsInboxMessage
  public let editing: Bool
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsInboxLabels

  /*
   * The message's age in words.
   */
  public let time: String

  public let contentVersion: Int

  public init(
    item: ShadowListUtilsInboxMessage, editing: Bool, theme: ShadowListUtilsTheme, labels: ShadowListUtilsInboxLabels
  ) {
    self.item = item
    self.editing = editing
    self.theme = theme
    self.labels = labels
    time = labels.time.format(dateMs: item.receivedAtMs, nowMs: ShadowListUtilsRelativeTimeLabels.nowMs)
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(editing)
    hasher.combine(time)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsInboxRowView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let typography = theme.typography
    let direction = ShadowListUtilsDirection(width: width)
    let top = spacing.md
    let avatarFrame = CGRect(x: Self.gutterWidth, y: top, width: Self.avatarSize, height: Self.avatarSize)
    let bodyX = avatarFrame.maxX + spacing.md
    let bodyWidth = max(1, width - bodyX - spacing.lg)

    // The age keeps its width at the trailing edge, the flag sits before it and the sender takes the rest.
    let time = typography.footnote.layout(self.time, width: bodyWidth, maxLines: 1)
    let flagWidth = item.flagged ? Self.flagSize + 2 * spacing.xs : 0
    let senderStyle = item.read ? typography.body : typography.body.with(.semibold)
    let sender = senderStyle.layout(item.sender, width: max(1, bodyWidth - time.size.width - flagWidth), maxLines: 1)
    let titleHeight = typography.body.scaledLineHeight()
    let timeFrame = CGRect(
      CGPoint(
        x: bodyX + bodyWidth - time.size.width,
        y: top + ShadowListUtilsPlatform.roundToPixel((titleHeight - typography.footnote.scaledLineHeight()) / 2)),
      time)
    let subject = typography.subhead.layout(item.subject, width: bodyWidth, maxLines: 1)
    let preview = typography.subhead.layout(item.preview, width: bodyWidth, maxLines: Self.previewLines)
    let subjectFrame = CGRect(CGPoint(x: bodyX, y: top + titleHeight), subject)
    let previewFrame = CGRect(CGPoint(x: bodyX, y: subjectFrame.minY + typography.subhead.scaledLineHeight()), preview)
    let unreadDotFrame = CGRect(
      x: (Self.gutterWidth - Self.unreadDotSize) / 2, y: top + Self.unreadDotTop, width: Self.unreadDotSize, height: Self.unreadDotSize)
    let checkFrame = CGRect(
      x: (Self.gutterWidth - Self.checkSize) / 2, y: top + Self.checkTop, width: Self.checkSize, height: Self.checkSize)
    let flagFrame = CGRect(
      x: timeFrame.minX - spacing.xs - Self.flagSize, y: top + (titleHeight - Self.flagSize) / 2, width: Self.flagSize, height: Self.flagSize)
    return ShadowListUtilsInboxRowLayout(
      width: width, height: max(avatarFrame.maxY, previewFrame.maxY) + spacing.md, unreadDotFrame: direction.place(unreadDotFrame),
      checkFrame: direction.place(checkFrame), avatarFrame: direction.place(avatarFrame), sender: sender,
      senderFrame: direction.place(CGRect(CGPoint(x: bodyX, y: top), sender)), flagFrame: direction.place(flagFrame), time: time,
      timeFrame: direction.place(timeFrame), subject: subject, subjectFrame: direction.place(subjectFrame), preview: preview,
      previewFrame: direction.place(previewFrame))
  }

  static let avatarSize: CGFloat = 40
  static let gutterWidth: CGFloat = 28
  static let unreadDotSize: CGFloat = 10
  static let checkSize: CGFloat = 22
  static let flagSize: CGFloat = 8
  private static let unreadDotTop: CGFloat = 16
  private static let checkTop: CGFloat = 9
  private static let previewLines = 2
}

public final class ShadowListUtilsInboxRowLayout: ShadowListUtilsRowLayout {
  public let unreadDotFrame: CGRect
  public let checkFrame: CGRect
  public let avatarFrame: CGRect
  public let sender: ShadowListKitTextLayout
  public let senderFrame: CGRect
  public let flagFrame: CGRect
  public let time: ShadowListKitTextLayout
  public let timeFrame: CGRect
  public let subject: ShadowListKitTextLayout
  public let subjectFrame: CGRect
  public let preview: ShadowListKitTextLayout
  public let previewFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, unreadDotFrame: CGRect, checkFrame: CGRect, avatarFrame: CGRect, sender: ShadowListKitTextLayout,
    senderFrame: CGRect, flagFrame: CGRect, time: ShadowListKitTextLayout, timeFrame: CGRect, subject: ShadowListKitTextLayout,
    subjectFrame: CGRect, preview: ShadowListKitTextLayout, previewFrame: CGRect
  ) {
    self.unreadDotFrame = unreadDotFrame
    self.checkFrame = checkFrame
    self.avatarFrame = avatarFrame
    self.sender = sender
    self.senderFrame = senderFrame
    self.flagFrame = flagFrame
    self.time = time
    self.timeFrame = timeFrame
    self.subject = subject
    self.subjectFrame = subjectFrame
    self.preview = preview
    self.previewFrame = previewFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsInboxRowView: ShadowListUtilsRowView {
  private let unreadDot = UIView()
  private let check = ShadowListUtilsCheckCircle()
  private let avatar = ShadowListUtilsAvatar(size: ShadowListUtilsInboxRow.avatarSize, theme: .system)
  private let sender = ShadowListKitTextView(frame: .zero)
  private let flag = UIView()
  private let time = ShadowListKitTextView(frame: .zero)
  private let subject = ShadowListKitTextView(frame: .zero)
  private let preview = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    unreadDot.layer.cornerRadius = ShadowListUtilsInboxRow.unreadDotSize / 2
    flag.layer.cornerRadius = Self.flagRadius
    [unreadDot, check, avatar, sender, flag, time, subject, preview].forEach(addSubview)
  }

  /*
   * The list tells its cells when it edits. A row made with editing shows the circle too, for a
   * list that is not a ShadowListUtilsInboxList.
   */
  private var showsCheck: Bool {
    isEditing || (row as? ShadowListUtilsInboxRow)?.editing == true
  }

  override var showsPressedBackground: Bool {
    isHighlighted || (showsCheck && isSelected)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsInboxRow else { return }
    let colors = row.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = colors.fill
    unreadDot.backgroundColor = colors.accent
    flag.backgroundColor = colors.orange
    flag.isHidden = !row.item.flagged
    check.colors = colors
    avatar.theme = row.theme
    avatar.show(name: row.item.sender, imageUrl: row.item.avatarUrl)
    updateState()
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsInboxRow, let layout = layout as? ShadowListUtilsInboxRowLayout else { return }
    let colors = row.theme.colors
    unreadDot.frame = layout.unreadDotFrame
    check.frame = layout.checkFrame
    avatar.frame = layout.avatarFrame
    sender.show(layout.sender, colors.label)
    sender.frame = layout.senderFrame
    flag.frame = layout.flagFrame
    time.show(layout.time, colors.secondaryLabel)
    time.frame = layout.timeFrame
    subject.show(layout.subject, colors.label)
    subject.frame = layout.subjectFrame
    preview.show(layout.preview, colors.secondaryLabel)
    preview.frame = layout.previewFrame
  }

  public override func setSelected(_ selected: Bool, animated: Bool) {
    super.setSelected(selected, animated: animated)
    updateState()
    updateBackground()
  }

  public override func setEditing(_ editing: Bool, animated: Bool) {
    super.setEditing(editing, animated: animated)
    updateState()
    updateBackground()
  }

  /*
   * The gutter shows the unread dot, or the selection circle while editing. The label reads the
   * row's states before its text.
   */
  private func updateState() {
    guard let row = row as? ShadowListUtilsInboxRow else { return }
    let item = row.item
    let editing = showsCheck
    unreadDot.isHidden = editing || item.read
    check.isHidden = !editing
    check.isOn = isSelected
    let states = [
      item.read ? nil : row.labels.unreadState,
      item.flagged ? row.labels.flaggedState : nil,
      editing && isSelected ? row.labels.selected : nil,
    ]
    accessibilityTraits = editing && isSelected ? .selected : []
    accessibilityLabel = (states + [item.sender, item.subject, item.preview]).compactMap { $0 }.joined(separator: ", ")
  }

  private static let flagRadius: CGFloat = 2
}

/*
 * The selection circle of a row in a list that is editing: an outline, or a filled circle with
 * a drawn check when on.
 */
final class ShadowListUtilsCheckCircle: ShadowListUtilsView {
  var colors: ShadowListUtilsThemeColors? {
    didSet { setNeedsDisplay() }
  }

  var isOn = false {
    didSet {
      if isOn != oldValue {
        setNeedsDisplay()
      }
    }
  }

  init() {
    super.init(frame: .zero)
    isOpaque = false
    backgroundColor = .clear
    contentMode = .redraw
    isUserInteractionEnabled = false
    isAccessibilityElement = false
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func draw(_ rect: CGRect) {
    guard let colors else { return }
    if isOn {
      colors.accent.setFill()
      UIBezierPath(ovalIn: bounds).fill()
      let check = ShadowListUtilsIconView.path(.check, size: Self.checkSize)
      check.apply(CGAffineTransform(translationX: bounds.midX - Self.checkSize / 2, y: bounds.midY - Self.checkSize / 2))
      colors.onAccent.setStroke()
      check.stroke()
      return
    }
    let outline = UIBezierPath(ovalIn: bounds.insetBy(dx: Self.borderWidth / 2, dy: Self.borderWidth / 2))
    outline.lineWidth = Self.borderWidth
    colors.tertiaryLabel.setStroke()
    outline.stroke()
  }

  private static let borderWidth: CGFloat = 1.5
  private static let checkSize: CGFloat = 14
}
