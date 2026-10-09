import ShadowListKit
import UIKit

/*
 * A row of a list. Its layout is a pure function of the row and the width, safe to compute off
 * the main thread, and a row view only copies the frames it holds.
 */
protocol Row {
  var key: String { get }
  var viewClass: RowView.Type { get }
  func layout(width: CGFloat) -> RowLayout
}

extension Row {
  var reuseIdentifier: String { NSStringFromClass(viewClass) }
}

class RowLayout {
  let width: CGFloat
  let height: CGFloat

  init(width: CGFloat, height: CGFloat) {
    self.width = width
    self.height = height
  }
}

/*
 * The content of one row, shared by every list engine. The engines only differ in how they
 * host it and where its layout comes from.
 */
class RowView: UIView {
  private(set) var row: Row?
  private(set) var rowLayout: RowLayout?

  required override init(frame: CGRect) {
    super.init(frame: frame)
    backgroundColor = Theme.background
  }

  required init?(coder: NSCoder) { fatalError() }

  /*
   * Show a row. With a layout the frames are taken as is. Without one the layout is computed
   * on the main thread the first time the view knows its width, like a self-sizing cell.
   */
  func configure(_ row: Row, layout: RowLayout?) {
    self.row = row
    rowLayout = layout
    fill(row)
    setNeedsLayout()
  }

  func ensureLayout(width: CGFloat) -> RowLayout? {
    guard let row, width > 0 else { return nil }
    if let rowLayout, rowLayout.width == width {
      return rowLayout
    }
    let layout = row.layout(width: width)
    rowLayout = layout
    return layout
  }

  override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: size.width, height: ensureLayout(width: size.width)?.height ?? 0)
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    if let row, let layout = ensureLayout(width: bounds.width) {
      apply(row, layout)
    }
  }

  /*
   * Set the content, like text and images, without frames.
   */
  func fill(_ row: Row) {}

  /*
   * Set the frames from the layout.
   */
  func apply(_ row: Row, _ layout: RowLayout) {}
}

/*
 * Precomputed layouts by row key and width, filled ahead on a background queue.
 */
final class LayoutCache {
  private var layouts: [String: RowLayout] = [:]
  private var lock = os_unfair_lock()
  private let queue = DispatchQueue(label: "layout.prefetch", qos: .userInitiated)
  private var generation = 0

  // Set by the cost scenario while it times updates. Prefetch work would compete with them.
  var prefetchPaused = false

  func layout(_ row: Row, width: CGFloat) -> RowLayout {
    let key = row.key
    os_unfair_lock_lock(&lock)
    let found = layouts[key]
    os_unfair_lock_unlock(&lock)
    if let found, found.width == width {
      return found
    }
    let layout = row.layout(width: width)
    os_unfair_lock_lock(&lock)
    layouts[key] = layout
    os_unfair_lock_unlock(&lock)
    return layout
  }

  /*
   * Lay out rows the list has not reached yet. Rows nearest the given index go first.
   */
  /*
   * Run work on the main thread once the prefetch queued so far finished.
   */
  func whenIdle(_ work: @escaping () -> Void) {
    queue.async { DispatchQueue.main.async(execute: work) }
  }

  func prefetch(_ rows: [Row], width: CGFloat, around index: Int = 0) {
    guard width > 0, !prefetchPaused else { return }
    os_unfair_lock_lock(&lock)
    generation += 1
    let current = generation
    os_unfair_lock_unlock(&lock)
    queue.async { [weak self] in
      // Rows nearest the index first, sorted here and not on the main thread.
      let ordered = rows.indices.sorted { abs($0 - index) < abs($1 - index) }.map { rows[$0] }
      for row in ordered {
        guard let self else { return }
        os_unfair_lock_lock(&self.lock)
        let stale = self.generation != current
        let done = self.layouts[row.key]?.width == width
        os_unfair_lock_unlock(&self.lock)
        if stale { return }
        if !done {
          _ = self.layout(row, width: width)
        }
      }
    }
  }

  func invalidate(_ key: String) {
    os_unfair_lock_lock(&lock)
    layouts.removeValue(forKey: key)
    os_unfair_lock_unlock(&lock)
  }
}

// MARK: - Shared pieces

/*
 * A colored circle with initials. The initials are typeset once per name and size.
 */
final class AvatarView: UIView {
  private static var cache: [String: ShadowListKitTextLayout] = [:]
  private static var lock = os_unfair_lock()

  static func initials(_ author: Author, size: CGFloat) -> ShadowListKitTextLayout {
    let key = "\(author.initials)#\(size)"
    os_unfair_lock_lock(&lock)
    defer { os_unfair_lock_unlock(&lock) }
    if let layout = cache[key] {
      return layout
    }
    let style = TextStyle(floor(size * 0.43), .semibold, lineHeight: ceil(floor(size * 0.43) * 1.2), kern: 0)
    let layout = style.layout(author.initials, width: size)
    cache[key] = layout
    return layout
  }

  private let size: CGFloat
  private let text = ShadowListKitTextView.make()

  init(size: CGFloat) {
    self.size = size
    super.init(frame: CGRect(x: 0, y: 0, width: size, height: size))
    layer.cornerRadius = size / 2
    addSubview(text)
  }

  required init?(coder: NSCoder) { fatalError() }

  func show(_ author: Author) {
    layer.backgroundColor = author.color.cgColor
    let layout = Self.initials(author, size: size)
    text.show(layout, Theme.label)
    text.frame = CGRect(
      x: ((size - layout.size.width) / 2).rounded(), y: ((size - layout.size.height) / 2).rounded(),
      width: layout.size.width, height: layout.size.height)
  }
}

extension CGRect {
  init(_ origin: CGPoint, _ layout: ShadowListKitTextLayout) {
    self.init(origin: origin, size: layout.size)
  }
}

// MARK: - Feed

struct FeedRow: Row {
  let post: FeedPost
  var key: String { post.id }
  var viewClass: RowView.Type { FeedRowView.self }

  static let nameStyle = TextStyle.subhead.with(.semibold)

  func layout(width: CGFloat) -> RowLayout {
    let contentX: CGFloat = 16 + 40 + 12
    let contentWidth = width - contentX - 16
    let top: CGFloat = 12
    let layout = FeedLayout(width: width, height: 0)
    layout.date = TextStyle.subhead.layout("· \(post.time)", width: contentWidth, maxLines: 1)
    var available = max(0, contentWidth - layout.date.size.width - 8)
    layout.name = Self.nameStyle.layout(post.author.name, width: available, maxLines: 1)
    available -= layout.name.size.width
    layout.handle = TextStyle.subhead.layout(post.handle, width: max(1, available), maxLines: 1)
    layout.nameFrame = CGRect(CGPoint(x: contentX, y: top), layout.name)
    layout.handleFrame = CGRect(CGPoint(x: layout.nameFrame.maxX + 4, y: top), layout.handle)
    layout.dateFrame = CGRect(CGPoint(x: layout.handleFrame.maxX + 4, y: top), layout.date)
    layout.body = TextStyle.subhead.layout(post.text, width: contentWidth)
    layout.bodyFrame = CGRect(x: contentX, y: top + 22, width: contentWidth, height: layout.body.size.height)
    let imagesTop = layout.bodyFrame.maxY + 12
    if post.images.count > 1 {
      layout.carousel = CGRect(x: contentX - 4, y: imagesTop, width: contentWidth + 8, height: 200)
    } else {
      layout.image = CGRect(x: contentX, y: imagesTop, width: contentWidth, height: 200)
    }
    return layout.finished(height: imagesTop + 200 + 12)
  }
}

final class FeedLayout: RowLayout {
  var name: ShadowListKitTextLayout!
  var handle: ShadowListKitTextLayout!
  var date: ShadowListKitTextLayout!
  var body: ShadowListKitTextLayout!
  var nameFrame = CGRect.zero
  var handleFrame = CGRect.zero
  var dateFrame = CGRect.zero
  var bodyFrame = CGRect.zero
  var image = CGRect.zero
  var carousel = CGRect.zero

  func finished(height: CGFloat) -> FeedLayout {
    let done = FeedLayout(width: width, height: height)
    (done.name, done.handle, done.date, done.body) = (name, handle, date, body)
    (done.nameFrame, done.handleFrame, done.dateFrame, done.bodyFrame, done.image, done.carousel) =
      (nameFrame, handleFrame, dateFrame, bodyFrame, image, carousel)
    return done
  }
}

final class FeedRowView: RowView {
  private let avatar = AvatarView(size: 40)
  private let name = ShadowListKitTextView.make()
  private let handle = ShadowListKitTextView.make()
  private let date = ShadowListKitTextView.make()
  private let body = ShadowListKitTextView.make()
  private let image = RemoteImageView()
  private let carousel = UIScrollView()
  private var carouselImages: [RemoteImageView] = []
  private let separator = UIView()

  required init(frame: CGRect) {
    super.init(frame: frame)
    image.backgroundColor = Theme.elevated2
    image.layer.cornerRadius = 16
    carousel.showsHorizontalScrollIndicator = false
    carousel.contentInset = UIEdgeInsets(top: 0, left: 4, bottom: 0, right: 4)
    carousel.clipsToBounds = false
    separator.backgroundColor = Theme.separator
    [avatar, name, handle, date, body, image, carousel, separator].forEach(addSubview)
  }

  required init?(coder: NSCoder) { fatalError() }

  override func fill(_ row: Row) {
    let post = (row as! FeedRow).post
    avatar.show(post.author)
    let multi = post.images.count > 1
    image.isHidden = multi
    carousel.isHidden = !multi
    if multi {
      while carouselImages.count < post.images.count {
        let view = RemoteImageView()
        view.backgroundColor = Theme.elevated2
        view.layer.cornerRadius = 16
        carousel.addSubview(view)
        carouselImages.append(view)
      }
      for (index, view) in carouselImages.enumerated() {
        view.isHidden = index >= post.images.count
        view.frame = CGRect(x: 4 + CGFloat(index) * 288, y: 0, width: 280, height: 200)
        view.setImage(index < post.images.count ? post.images[index] : nil, size: view.bounds.size)
      }
      carousel.contentSize = CGSize(width: CGFloat(post.images.count) * 288, height: 200)
      carousel.contentOffset = CGPoint(x: -4, y: 0)
    }
  }

  override func apply(_ row: Row, _ layout: RowLayout) {
    let layout = layout as! FeedLayout
    let post = (row as! FeedRow).post
    avatar.frame.origin = CGPoint(x: 16, y: 12)
    name.show(layout.name, Theme.label)
    name.frame = layout.nameFrame
    handle.show(layout.handle, Theme.secondaryLabel)
    handle.frame = layout.handleFrame
    date.show(layout.date, Theme.secondaryLabel)
    date.frame = layout.dateFrame
    body.show(layout.body, Theme.label)
    body.frame = layout.bodyFrame
    if !image.isHidden {
      image.frame = layout.image
      image.setImage(post.images.first, size: layout.image.size)
    }
    carousel.frame = layout.carousel
    separator.frame = CGRect(x: 68, y: layout.height - Theme.hairline, width: layout.width - 68, height: Theme.hairline)
  }
}

// MARK: - Chat

struct ChatRow: Row {
  let message: ChatMessage
  let caption: String?
  var key: String { message.id }
  var viewClass: RowView.Type { ChatRowView.self }

  func layout(width: CGFloat) -> RowLayout {
    let own = message.isOwn
    let layout = ChatLayout(width: width, height: 0)
    let contentLeft: CGFloat = own ? 12 : 12 + 30 + 8
    var y: CGFloat = 2
    if !message.images.isEmpty {
      let blockHeight: CGFloat = message.images.count > 1 ? 240 + 2 : 320
      let x = own ? width - 12 - 240 : contentLeft
      layout.images = CGRect(x: x, y: y + 2, width: 240, height: message.images.count > 1 ? 240 : 320)
      y += 2 + blockHeight + 2
      if let caption {
        let text = TextStyle.caption.layout(caption, width: 240, maxLines: 1)
        layout.caption = text
        layout.captionFrame = CGRect(CGPoint(x: own ? width - 12 - text.size.width : contentLeft, y: y + 4), text)
        y += 4 + 16
      }
    } else {
      let maxColumn = 0.75 * (width - 24)
      if !own {
        let name = TextStyle.caption.layout(message.author.name, width: maxColumn, maxLines: 1)
        layout.name = name
        layout.nameFrame = CGRect(CGPoint(x: contentLeft + 12, y: y), name)
        y += 18
      }
      let text = TextStyle.body.layout(message.text ?? "", width: maxColumn - 28)
      layout.text = text
      var innerWidth = text.size.width
      var innerHeight = text.size.height
      if let caption {
        let captionText = TextStyle.caption.layout(caption, width: maxColumn - 28, maxLines: 1)
        layout.caption = captionText
        innerWidth = max(innerWidth, captionText.size.width)
        innerHeight += 4 + 16
      }
      let bubbleWidth = innerWidth + 28
      let bubbleX = own ? width - 12 - bubbleWidth : contentLeft
      layout.bubble = CGRect(x: bubbleX, y: y, width: bubbleWidth, height: innerHeight + 16)
      layout.textFrame = CGRect(x: bubbleX + 14, y: y + 8, width: innerWidth, height: text.size.height)
      if let captionText = layout.caption {
        layout.captionFrame = CGRect(CGPoint(x: bubbleX + 14, y: layout.textFrame.maxY + 4), captionText)
      }
      y = layout.bubble.maxY
    }
    let height = y + 2
    if !own {
      layout.avatar = CGRect(x: 12, y: height - 2 - 2 - 30, width: 30, height: 30)
    }
    return layout.finished(height: height)
  }
}

final class ChatLayout: RowLayout {
  var name: ShadowListKitTextLayout?
  var text: ShadowListKitTextLayout?
  var caption: ShadowListKitTextLayout?
  var avatar = CGRect.zero
  var nameFrame = CGRect.zero
  var bubble = CGRect.zero
  var textFrame = CGRect.zero
  var captionFrame = CGRect.zero
  var images = CGRect.zero

  func finished(height: CGFloat) -> ChatLayout {
    let done = ChatLayout(width: width, height: height)
    (done.name, done.text, done.caption) = (name, text, caption)
    (done.avatar, done.nameFrame, done.bubble, done.textFrame, done.captionFrame, done.images) =
      (avatar, nameFrame, bubble, textFrame, captionFrame, images)
    return done
  }
}

/*
 * Stretchable bubble images, drawn once: radius 18 with a radius 5 tail corner.
 */
enum BubbleImages {
  static let own = make(tailRight: true)
  static let other = make(tailRight: false)

  private static func make(tailRight: Bool) -> UIImage {
    let size = CGSize(width: 40, height: 40)
    let image = UIGraphicsImageRenderer(size: size).image { _ in
      let rect = CGRect(origin: .zero, size: size)
      let path = UIBezierPath()
      let big: CGFloat = 18
      let small: CGFloat = 5
      let bottomRight = tailRight ? small : big
      let bottomLeft = tailRight ? big : small
      path.move(to: CGPoint(x: big, y: 0))
      path.addLine(to: CGPoint(x: rect.maxX - big, y: 0))
      path.addArc(withCenter: CGPoint(x: rect.maxX - big, y: big), radius: big, startAngle: -.pi / 2, endAngle: 0, clockwise: true)
      path.addLine(to: CGPoint(x: rect.maxX, y: rect.maxY - bottomRight))
      path.addArc(withCenter: CGPoint(x: rect.maxX - bottomRight, y: rect.maxY - bottomRight), radius: bottomRight, startAngle: 0, endAngle: .pi / 2, clockwise: true)
      path.addLine(to: CGPoint(x: bottomLeft, y: rect.maxY))
      path.addArc(withCenter: CGPoint(x: bottomLeft, y: rect.maxY - bottomLeft), radius: bottomLeft, startAngle: .pi / 2, endAngle: .pi, clockwise: true)
      path.addLine(to: CGPoint(x: 0, y: big))
      path.addArc(withCenter: CGPoint(x: big, y: big), radius: big, startAngle: .pi, endAngle: 1.5 * .pi, clockwise: true)
      UIColor.black.setFill()
      path.fill()
    }
    return image.resizableImage(withCapInsets: UIEdgeInsets(top: 19, left: 19, bottom: 19, right: 19)).withRenderingMode(.alwaysTemplate)
  }
}

final class ChatRowView: RowView {
  private let avatar = AvatarView(size: 30)
  private let name = ShadowListKitTextView.make()
  private let bubble = UIImageView()
  private let text = ShadowListKitTextView.make()
  private let caption = ShadowListKitTextView.make()
  private let imageBlock = UIView()
  private var images: [RemoteImageView] = []

  required init(frame: CGRect) {
    super.init(frame: frame)
    for _ in 0..<4 {
      let view = RemoteImageView()
      view.backgroundColor = Theme.elevated2
      imageBlock.addSubview(view)
      images.append(view)
    }
    [avatar, name, bubble, text, caption, imageBlock].forEach(addSubview)
  }

  required init?(coder: NSCoder) { fatalError() }

  override func fill(_ row: Row) {
    let message = (row as! ChatRow).message
    let own = message.isOwn
    avatar.isHidden = own
    avatar.show(message.author)
    let hasText = message.text != nil
    name.isHidden = own || !hasText
    bubble.isHidden = !hasText
    text.isHidden = !hasText
    bubble.image = own ? BubbleImages.own : BubbleImages.other
    bubble.tintColor = own ? Theme.accent : Theme.elevated2
    imageBlock.isHidden = message.images.isEmpty
  }

  override func apply(_ row: Row, _ layout: RowLayout) {
    let layout = layout as! ChatLayout
    let message = (row as! ChatRow).message
    let own = message.isOwn
    avatar.frame = layout.avatar
    name.show(layout.name, Theme.secondaryLabel)
    name.frame = layout.nameFrame
    bubble.frame = layout.bubble
    text.show(layout.text, own ? Theme.onAccent : Theme.label)
    text.frame = layout.textFrame
    caption.isHidden = layout.caption == nil
    caption.show(layout.caption, message.text != nil && own ? Theme.onAccent.withAlphaComponent(0.65) : Theme.secondaryLabel)
    caption.frame = layout.captionFrame
    imageBlock.frame = layout.images
    guard !message.images.isEmpty else { return }
    if message.images.count > 1 {
      for (index, view) in images.enumerated() {
        view.isHidden = false
        view.layer.cornerRadius = 8
        view.frame = CGRect(x: CGFloat(index % 2) * 121, y: CGFloat(index / 2) * 121, width: 119, height: 119)
        view.setImage(message.images[index], size: CGSize(width: 119, height: 136.85))
      }
    } else {
      images[0].isHidden = false
      images[0].layer.cornerRadius = 16
      images[0].frame = CGRect(x: 0, y: 0, width: 240, height: 320)
      images[0].setImage(message.images[0], size: CGSize(width: 240, height: 368))
      images[1...].forEach { $0.isHidden = true }
    }
  }
}

// MARK: - Directory

struct ContactRow: Row {
  let contact: Contact
  // Rows other than the last in a section are followed by a separator strip.
  let separatorBelow: Bool
  var key: String { contact.id }
  var viewClass: RowView.Type { ContactRowView.self }

  func layout(width: CGFloat) -> RowLayout {
    let textWidth = width - 68 - 12
    let layout = ContactLayout(width: width, height: 67 + (separatorBelow ? Theme.hairline : 0))
    layout.name = TextStyle.body.layout(contact.author.name, width: textWidth, maxLines: 1)
    layout.subtitle = TextStyle.subhead.layout(contact.subtitle, width: textWidth, maxLines: 1)
    return layout
  }
}

final class ContactLayout: RowLayout {
  var name: ShadowListKitTextLayout!
  var subtitle: ShadowListKitTextLayout!
}

final class ContactRowView: RowView {
  private let avatar = AvatarView(size: 40)
  private let name = ShadowListKitTextView.make()
  private let subtitle = ShadowListKitTextView.make()
  private let separator = UIView()
  private let strip = UIView()

  required init(frame: CGRect) {
    super.init(frame: frame)
    separator.backgroundColor = Theme.separator
    strip.backgroundColor = Theme.separator
    [avatar, name, subtitle, separator, strip].forEach(addSubview)
  }

  required init?(coder: NSCoder) { fatalError() }

  override func fill(_ row: Row) {
    let row = row as! ContactRow
    avatar.show(row.contact.author)
    strip.isHidden = !row.separatorBelow
  }

  override func apply(_ row: Row, _ layout: RowLayout) {
    let layout = layout as! ContactLayout
    let width = layout.width
    avatar.frame.origin = CGPoint(x: 16, y: 13.5)
    name.show(layout.name, Theme.label)
    name.frame = CGRect(CGPoint(x: 68, y: 12), layout.name)
    subtitle.show(layout.subtitle, Theme.secondaryLabel)
    subtitle.frame = CGRect(CGPoint(x: 68, y: 35), layout.subtitle)
    separator.frame = CGRect(x: 68, y: 67 - Theme.hairline, width: width - 68, height: Theme.hairline)
    strip.frame = CGRect(x: 68, y: 67, width: width - 68, height: Theme.hairline)
  }
}

struct SectionHeaderRow: Row {
  let title: String
  let count: Int
  var key: String { "section-\(title)" }
  var viewClass: RowView.Type { SectionHeaderView.self }

  func layout(width: CGFloat) -> RowLayout {
    let layout = SectionHeaderLayout(width: width, height: 38)
    layout.title = TextStyle.footnote.with(.semibold).layout(title.uppercased(), width: width / 2, maxLines: 1)
    layout.count = TextStyle.footnote.layout("\(count)", width: width / 2, maxLines: 1)
    return layout
  }
}

final class SectionHeaderLayout: RowLayout {
  var title: ShadowListKitTextLayout!
  var count: ShadowListKitTextLayout!
}

final class SectionHeaderView: RowView {
  private let title = ShadowListKitTextView.make()
  private let count = ShadowListKitTextView.make()

  required init(frame: CGRect) {
    super.init(frame: frame)
    backgroundColor = Theme.elevated
    [title, count].forEach(addSubview)
  }

  required init?(coder: NSCoder) { fatalError() }

  override func apply(_ row: Row, _ layout: RowLayout) {
    let layout = layout as! SectionHeaderLayout
    title.show(layout.title, Theme.secondaryLabel)
    title.frame = CGRect(CGPoint(x: 16, y: 10), layout.title)
    count.show(layout.count, Theme.tertiaryLabel)
    count.frame = CGRect(CGPoint(x: layout.width - 16 - layout.count.size.width, y: 10), layout.count)
  }
}

// MARK: - Gallery

struct PhotoRow: Row {
  let photo: Photo
  var key: String { photo.id }
  var viewClass: RowView.Type { PhotoCardView.self }

  func layout(width: CGFloat) -> RowLayout {
    let imageWidth = width - 12
    let imageHeight = (imageWidth * photo.aspect).rounded()
    let title = TextStyle.subhead.layout(photo.title, width: imageWidth - 8, maxLines: 2)
    let layout = PhotoLayout(width: width, height: imageHeight + 8 + title.size.height + 12)
    layout.image = CGRect(x: 6, y: 0, width: imageWidth, height: imageHeight)
    layout.title = title
    layout.titleFrame = CGRect(CGPoint(x: 10, y: imageHeight + 8), title)
    return layout
  }
}

final class PhotoLayout: RowLayout {
  var image = CGRect.zero
  var title: ShadowListKitTextLayout!
  var titleFrame = CGRect.zero
}

final class PhotoCardView: RowView {
  private let image = RemoteImageView()
  private let title = ShadowListKitTextView.make()

  required init(frame: CGRect) {
    super.init(frame: frame)
    image.backgroundColor = Theme.elevated2
    image.layer.cornerRadius = 12
    [image, title].forEach(addSubview)
  }

  required init?(coder: NSCoder) { fatalError() }

  override func apply(_ row: Row, _ layout: RowLayout) {
    let layout = layout as! PhotoLayout
    image.frame = layout.image
    image.setImage((row as! PhotoRow).photo.url, size: layout.image.size)
    title.show(layout.title, Theme.label)
    title.frame = layout.titleFrame
  }
}

// MARK: - Templates

final class ListFooterView: UIView {
  private let label = UILabel.make()

  init(_ text: String) {
    super.init(frame: .zero)
    backgroundColor = Theme.background
    label.attributedText = TextStyle.footnote.string(text, Theme.secondaryLabel, alignment: .center)
    addSubview(label)
    frame.size.height = 58
  }

  required init?(coder: NSCoder) { fatalError() }

  override func layoutSubviews() {
    super.layoutSubviews()
    label.frame = bounds.insetBy(dx: 16, dy: 20)
  }
}

final class SpinnerView: UIView {
  private let spinner = UIActivityIndicatorView(style: .medium)

  override init(frame: CGRect) {
    super.init(frame: frame)
    spinner.color = Theme.secondaryLabel
    spinner.startAnimating()
    addSubview(spinner)
    self.frame.size.height = 52
  }

  required init?(coder: NSCoder) { fatalError() }

  override func layoutSubviews() {
    super.layoutSubviews()
    spinner.center = CGPoint(x: bounds.midX, y: bounds.midY)
  }
}

final class ListHeaderView: UIView {
  private let title = UILabel.make(lines: 1)
  private let subtitle = UILabel.make(lines: 1)

  init(title: String, subtitle: String) {
    super.init(frame: .zero)
    backgroundColor = Theme.background
    self.title.attributedText = TextStyle.largeTitle.string(title)
    self.subtitle.attributedText = TextStyle.subhead.string(subtitle, Theme.secondaryLabel)
    addSubview(self.title)
    addSubview(self.subtitle)
    frame.size.height = 4 + 41 + 2 + 20 + 12
  }

  required init?(coder: NSCoder) { fatalError() }

  override func layoutSubviews() {
    super.layoutSubviews()
    title.frame = CGRect(x: 16, y: 4, width: bounds.width - 32, height: 41)
    subtitle.frame = CGRect(x: 16, y: 47, width: bounds.width - 32, height: 20)
  }
}
