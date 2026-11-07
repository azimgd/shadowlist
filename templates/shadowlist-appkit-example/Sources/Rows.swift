import AppKit
import ShadowListKit

/*
 * A feed post, a pure function of its generation index like the UIKit example's.
 */
struct FeedPost {
  let key: String
  let name: String
  let text: String

  init(index: Int) {
    key = "post-\(index)"
    name = FixtureStrings.characterNames[index % FixtureStrings.characterNames.count]
    text = FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.count]
  }
}

/*
 * A post's text typeset for one width, made once per key and width. The row's height comes from
 * here, before any cell exists.
 */
final class FeedLayout {
  static let padding: CGFloat = 12
  static let avatar: CGFloat = 36
  static let gap: CGFloat = 10
  static let nameFont = NSFont.systemFont(ofSize: 13, weight: .semibold)
  static let bodyFont = NSFont.systemFont(ofSize: 13)

  let width: CGFloat
  let name: ShadowListKitTextLayout
  let body: ShadowListKitTextLayout
  let height: CGFloat

  init(post: FeedPost, width: CGFloat) {
    self.width = width
    let textWidth = max(40, width - FeedLayout.padding * 2 - FeedLayout.avatar - FeedLayout.gap)
    name = ShadowListKitTextLayout(
      string: post.name, font: FeedLayout.nameFont, lineHeight: 16, kern: 0, width: textWidth, maximumLines: 1, alignment: .natural)
    body = ShadowListKitTextLayout(
      string: post.text, font: FeedLayout.bodyFont, lineHeight: 17, kern: 0, width: textWidth, maximumLines: 0, alignment: .natural)
    let text = name.size.height + 4 + body.size.height
    height = ceil(FeedLayout.padding * 2 + max(FeedLayout.avatar, text))
  }
}

/*
 * A colored circle behind the author's initial.
 */
final class AvatarView: NSView {
  var color = NSColor.systemBlue {
    didSet { needsDisplay = true }
  }

  override init(frame: NSRect) {
    super.init(frame: frame)
    wantsLayer = true
  }

  required init?(coder: NSCoder) { fatalError() }

  override var wantsUpdateLayer: Bool { true }

  override func updateLayer() {
    layer?.cornerRadius = bounds.height / 2
    layer?.backgroundColor = color.cgColor
  }
}

/*
 * A post: avatar, name and body, laid out by hand from its FeedLayout. Its background shows the
 * selection, emphasized or not, the press and the hover.
 */
final class FeedCell: ShadowListKitListCell {
  let avatarView = AvatarView()
  let nameView = ShadowListKitTextView()
  let bodyView = ShadowListKitTextView()
  private(set) var key = ""
  private(set) var name = ""
  private var textLayout: FeedLayout?

  override init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    bodyView.textColor = .secondaryLabelColor
    addSubview(avatarView)
    addSubview(nameView)
    addSubview(bodyView)
  }

  func configure(_ post: FeedPost, layout: FeedLayout) {
    key = post.key
    name = post.name
    textLayout = layout
    avatarView.color = FeedCell.color(of: post.name)
    nameView.textLayout = layout.name
    bodyView.textLayout = layout.body
    needsLayout = true
  }

  override func updateLayer() {
    let color: NSColor
    if isSelected {
      color = isEmphasized ? .selectedContentBackgroundColor : .unemphasizedSelectedContentBackgroundColor
    } else if isHighlighted {
      color = NSColor.textBackgroundColor.blended(withFraction: 0.12, of: .labelColor) ?? .textBackgroundColor
    } else if isHovered {
      color = NSColor.textBackgroundColor.blended(withFraction: 0.05, of: .labelColor) ?? .textBackgroundColor
    } else {
      color = .textBackgroundColor
    }
    layer?.backgroundColor = color.cgColor
    let onAccent = isSelected && isEmphasized
    nameView.textColor = onAccent ? .alternateSelectedControlTextColor : .labelColor
    bodyView.textColor = onAccent ? .alternateSelectedControlTextColor : .secondaryLabelColor
  }

  override func setSelected(_ selected: Bool, animated: Bool) {
    super.setSelected(selected, animated: animated)
    needsDisplay = true
  }

  override func setHighlighted(_ highlighted: Bool, animated: Bool) {
    super.setHighlighted(highlighted, animated: animated)
    needsDisplay = true
  }

  override func setHovered(_ hovered: Bool, animated: Bool) {
    super.setHovered(hovered, animated: animated)
    needsDisplay = true
  }

  override func setEmphasized(_ emphasized: Bool, animated: Bool) {
    super.setEmphasized(emphasized, animated: animated)
    needsDisplay = true
  }

  override func viewDidChangeEffectiveAppearance() {
    super.viewDidChangeEffectiveAppearance()
    needsDisplay = true
  }

  override func layout() {
    super.layout()
    guard let textLayout else { return }
    let padding = FeedLayout.padding
    let x = padding + FeedLayout.avatar + FeedLayout.gap
    avatarView.frame = NSRect(x: padding, y: padding, width: FeedLayout.avatar, height: FeedLayout.avatar)
    nameView.frame = NSRect(x: x, y: padding, width: textLayout.name.size.width, height: textLayout.name.size.height)
    bodyView.frame = NSRect(
      x: x, y: padding + textLayout.name.size.height + 4, width: textLayout.body.size.width, height: textLayout.body.size.height)
  }

  static func color(of name: String) -> NSColor {
    let palette: [NSColor] = [.systemBlue, .systemPink, .systemOrange, .systemTeal, .systemPurple, .systemGreen]
    let hash = name.unicodeScalars.reduce(0) { ($0 * 31 + Int($1.value)) % 2_147_483_647 }
    return palette[hash % palette.count]
  }
}

/*
 * A row of a grid: a fixed row number and tiles the host scrolls sideways by shift.
 */
final class GridRowCell: ShadowListKitListCell {
  static let numberWidth: CGFloat = 56
  static let columnWidth: CGFloat = 96
  static let font = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .regular)

  private let numberView = ShadowListKitTextView()
  private var tiles: [ShadowListKitTextView] = []
  private(set) var gridRow = -1
  var columnCount = 40
  var shift: CGFloat = 0 {
    didSet {
      if shift != oldValue { needsLayout = true }
    }
  }

  override init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    numberView.textColor = .secondaryLabelColor
    addSubview(numberView)
  }

  func configure(row: Int) {
    gridRow = row
    numberView.textLayout = GridRowCell.text("\(row + 1)")
    needsLayout = true
  }

  override func updateLayer() {
    layer?.backgroundColor = (gridRow % 2 == 0 ? NSColor.textBackgroundColor : NSColor.unemphasizedSelectedContentBackgroundColor).cgColor
  }

  /*
   * The first tile's left edge on screen, after the shift.
   */
  var firstTileX: CGFloat? {
    tiles.filter { !$0.isHidden }.map(\.frame.minX).min()
  }

  override func layout() {
    super.layout()
    let height = bounds.height
    numberView.frame = NSRect(x: 8, y: (height - 16) / 2, width: GridRowCell.numberWidth - 16, height: 16)
    let width = GridRowCell.columnWidth
    let first = max(0, Int(floor(shift / width)))
    let last = min(columnCount - 1, Int(floor((shift + bounds.width - GridRowCell.numberWidth) / width)))
    var used = 0
    if first <= last {
      for column in first...last {
        if used == tiles.count {
          let tile = ShadowListKitTextView()
          tiles.append(tile)
          addSubview(tile, positioned: .below, relativeTo: numberView)
        }
        let tile = tiles[used]
        tile.isHidden = false
        tile.textLayout = GridRowCell.text("R\(gridRow + 1)C\(column + 1)")
        tile.frame = NSRect(x: GridRowCell.numberWidth + CGFloat(column) * width - shift + 6, y: (height - 16) / 2, width: width - 12, height: 16)
        used += 1
      }
    }
    for tile in tiles[used...] {
      tile.isHidden = true
    }
  }

  private static var cache: [String: ShadowListKitTextLayout] = [:]

  static func text(_ string: String) -> ShadowListKitTextLayout {
    if let layout = cache[string] {
      return layout
    }
    let layout = ShadowListKitTextLayout(string: string, font: font, lineHeight: 16, kern: 0, width: columnWidth, maximumLines: 1, alignment: .natural)
    cache[string] = layout
    return layout
  }
}
