import ShadowListKit
import UIKit

// MARK: - Reorder

struct ReorderRow: Row {
  let contact: Contact
  var key: String { contact.id }
  var viewClass: RowView.Type { ReorderRowView.self }

  func layout(width: CGFloat) -> RowLayout {
    let textWidth = width - 68 - 16 - 20 - 12
    let layout = ContactLayout(width: width, height: 67)
    layout.name = TextStyle.body.layout(contact.author.name, width: textWidth, maxLines: 1)
    layout.subtitle = TextStyle.subhead.layout(contact.subtitle, width: textWidth, maxLines: 1)
    return layout
  }
}

final class ReorderRowView: RowView {
  private static let grip = UIImage(systemName: "line.3.horizontal", withConfiguration: UIImage.SymbolConfiguration(pointSize: 17, weight: .regular))

  private let avatar = AvatarView(size: 40)
  private let name = ShadowListKitTextView.make()
  private let subtitle = ShadowListKitTextView.make()
  private let grip = UIImageView(image: ReorderRowView.grip)
  private let separator = UIView()

  required init(frame: CGRect) {
    super.init(frame: frame)
    grip.tintColor = Theme.tertiaryLabel
    grip.contentMode = .center
    separator.backgroundColor = Theme.separator
    [avatar, name, subtitle, grip, separator].forEach(addSubview)
  }

  required init?(coder: NSCoder) { fatalError() }

  override func fill(_ row: Row) {
    avatar.show((row as! ReorderRow).contact.author)
  }

  override func apply(_ row: Row, _ layout: RowLayout) {
    let layout = layout as! ContactLayout
    let width = layout.width
    avatar.frame.origin = CGPoint(x: 16, y: 13.5)
    name.show(layout.name, Theme.label)
    name.frame = CGRect(CGPoint(x: 68, y: 12), layout.name)
    subtitle.show(layout.subtitle, Theme.secondaryLabel)
    subtitle.frame = CGRect(CGPoint(x: 68, y: 35), layout.subtitle)
    grip.frame = CGRect(x: width - 16 - 20, y: 23.5, width: 20, height: 20)
    separator.frame = CGRect(x: 68, y: 67 - Theme.hairline, width: width - 68, height: Theme.hairline)
  }
}

// MARK: - Snap

struct SnapRow: Row {
  let index: Int
  /*
   * A quarter of the screen, like the React Native card.
   */
  let height: CGFloat
  var key: String { "snap-\(index)" }
  var viewClass: RowView.Type { SnapCardView.self }

  var title: String { FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.count] }
  var color: UIColor { Theme.avatarPalette[index % Theme.avatarPalette.count] }
  var image: URL { Fixtures.imageURL(index, width: 800) }

  func layout(width: CGFloat) -> RowLayout {
    let layout = SnapLayout(width: width, height: height)
    layout.card = CGRect(x: 8, y: 8, width: width - 16, height: height - 16)
    let title = TextStyle(20, .semibold, lineHeight: 25, kern: 0.38).layout(self.title, width: layout.card.width - 32, maxLines: 2)
    layout.title = title
    layout.caption = CGRect(x: 0, y: layout.card.height - title.size.height - 32, width: layout.card.width, height: title.size.height + 32)
    return layout
  }
}

final class SnapLayout: RowLayout {
  var card = CGRect.zero
  var caption = CGRect.zero
  var title: ShadowListKitTextLayout!
}

final class SnapCardView: RowView {
  private let card = UIView()
  private let image = RemoteImageView()
  private let caption = UIView()
  private let title = ShadowListKitTextView.make()

  required init(frame: CGRect) {
    super.init(frame: frame)
    card.layer.cornerRadius = 16
    card.clipsToBounds = true
    caption.backgroundColor = UIColor(white: 0, alpha: 0.35)
    caption.addSubview(title)
    [image, caption].forEach(card.addSubview)
    addSubview(card)
  }

  required init?(coder: NSCoder) { fatalError() }

  override func fill(_ row: Row) {
    card.backgroundColor = (row as! SnapRow).color
  }

  override func apply(_ row: Row, _ layout: RowLayout) {
    let layout = layout as! SnapLayout
    card.frame = layout.card
    image.frame = card.bounds
    image.setImage((row as! SnapRow).image, size: layout.card.size)
    caption.frame = layout.caption
    title.show(layout.title, .white)
    title.frame = CGRect(CGPoint(x: 16, y: 16), layout.title)
  }
}
