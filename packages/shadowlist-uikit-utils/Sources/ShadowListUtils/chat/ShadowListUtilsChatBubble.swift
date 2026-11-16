import ShadowListKit
import UIKit

// A separate module with SwiftPM, and part of this one with CocoaPods.
#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * A chat message. Own messages sit at the trailing edge in the accent color. A message from
 * someone else shows the sender's name and avatar unless showsAuthor is false, as in the rest
 * of a run from one author: the avatar's place stays empty and the bubbles line up. caption is
 * a small line inside the bubble, or under the images of a message without text. An empty
 * caption is no caption.
 *
 * The layout is the row's size prediction. The bubble's share of the row and its padding come
 * from the core.
 */
public struct ShadowListUtilsChatBubble: ShadowListUtilsRow {
  public let item: ShadowListUtilsChatMessage
  public let caption: String?
  public let showsAuthor: Bool
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsChatLabels
  public let contentVersion: Int

  public init(
    item: ShadowListUtilsChatMessage, caption: String?, showsAuthor: Bool, theme: ShadowListUtilsTheme, labels: ShadowListUtilsChatLabels
  ) {
    self.item = item
    self.caption = caption
    self.showsAuthor = showsAuthor
    self.theme = theme
    self.labels = labels
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(caption)
    hasher.combine(showsAuthor)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsChatBubbleView.self }

  private var text: String? {
    item.text.flatMap { $0.isEmpty ? nil : $0 }
  }

  /*
   * The caption the row shows: nil for an empty one.
   */
  var shownCaption: String? {
    caption.flatMap { $0.isEmpty ? nil : $0 }
  }

  /*
   * What the core sizes a text message from. The metrics are the theme's, with the line heights
   * text is laid out with, which follow the text size setting. A message with images still asks
   * as a text message, for the width of the bubble under them.
   */
  private func sizeSpec() -> ShadowListUtilsChatSizeSpec {
    let shape = ShadowListUtilsChatShape(
      hasText: true, hasImages: false, isOwn: ObjCBool(item.isOwn), showsAuthor: ObjCBool(showsAuthor),
      hasCaption: ObjCBool(shownCaption != nil), failed: ObjCBool(item.status == .failed))
    let metrics = ShadowListUtilsChatMetrics(
      spacingXxs: theme.spacing.xxs, spacingXs: theme.spacing.xs, spacingSm: theme.spacing.sm, spacingMd: theme.spacing.md,
      spacingLg: theme.spacing.lg, bodyFontSize: theme.typography.body.fontSize, bodyLineHeight: theme.typography.body.scaledLineHeight(),
      bodyLetterSpacing: theme.typography.body.letterSpacing, captionLineHeight: theme.typography.caption.scaledLineHeight())
    var spec = ShadowListUtilsChatSizeSpec()
    _ = ShadowListUtilsCore.chatMessageSizeSpec(&spec, shape: shape, metrics: metrics)
    return spec
  }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let typography = theme.typography
    let direction = ShadowListUtilsDirection(width: width)
    let own = item.isOwn
    let images = Array(item.images.prefix(Self.gridLimit))
    let caption = shownCaption
    let captionLine = typography.caption.scaledLineHeight()

    // Others keep the avatar's column whether or not the avatar shows.
    let leading = own ? spacing.md : spacing.md + Self.avatarSize + spacing.sm
    let trailing = width - spacing.md
    let spec = sizeSpec()
    let textWidth = max(1, spec.widthFraction * width - spec.insetWidth)
    let bubblePadding = SHADOWLIST_UTILS_CHAT_BUBBLE_PADDING_HORIZONTAL
    let columnLimit = textWidth + 2 * bubblePadding

    var result = Frames()
    var y = spacing.xxs
    var columnWidth: CGFloat = 0

    if !images.isEmpty {
      let size = Self.imagesSize(count: images.count, gap: spacing.xxs)
      result.imagesFrame = CGRect(x: own ? trailing - size.width : leading, y: y + spacing.xxs, width: size.width, height: size.height)
      y = result.imagesFrame.maxY + spacing.xxs
      columnWidth = size.width
    } else if !own, showsAuthor, text != nil {
      let name = typography.caption.layout(item.author.name, width: max(1, columnLimit - spacing.md), maxLines: 1)
      result.name = name
      result.nameFrame = CGRect(CGPoint(x: leading + spacing.md, y: y), name)
      y += captionLine + spacing.xxs
      columnWidth = name.size.width + spacing.md
    }

    if let text {
      let body = typography.body.layout(text, width: textWidth)
      var innerWidth = body.lineCount > 1 ? textWidth : body.size.width
      var innerHeight = body.size.height
      if let caption {
        let line = typography.caption.layout(caption, width: textWidth, maxLines: 1)
        result.caption = line
        innerWidth = max(innerWidth, line.size.width)
        innerHeight += spacing.xs + captionLine
      }
      let bubbleWidth = innerWidth + 2 * bubblePadding
      let bubbleX = ShadowListUtilsPlatform.roundToPixel(own ? trailing - bubbleWidth : leading)
      result.bubbleFrame = CGRect(x: bubbleX, y: y, width: bubbleWidth, height: innerHeight + 2 * spacing.sm)
      result.text = body
      result.textFrame = CGRect(CGPoint(x: bubbleX + bubblePadding, y: y + spacing.sm), body)
      if let line = result.caption {
        result.captionFrame = CGRect(CGPoint(x: bubbleX + bubblePadding, y: result.textFrame.maxY + spacing.xs), line)
      }
      y = result.bubbleFrame.maxY
      columnWidth = max(columnWidth, bubbleWidth)
    } else if let caption {
      // A caption without text sits under the images, on the side the message is on.
      let line = typography.caption.layout(caption, width: textWidth, maxLines: 1)
      result.caption = line
      result.captionFrame = CGRect(CGPoint(x: own ? trailing - line.size.width : leading, y: y + spacing.xs), line)
      columnWidth = max(columnWidth, line.size.width)
      y = result.captionFrame.minY + captionLine
    }

    if item.status == .failed {
      // The failed line ends where the column of the message ends.
      let line = typography.caption.layout(labels.failed, width: columnLimit, maxLines: 1)
      columnWidth = max(columnWidth, line.size.width)
      let columnTrailing = own ? trailing : min(trailing, leading + columnWidth)
      result.failed = line
      result.failedFrame = CGRect(CGPoint(x: columnTrailing - line.size.width, y: y + spacing.xs), line)
      y = result.failedFrame.minY + captionLine
    }
    if images.count > 1 {
      y += spacing.xxs
    }

    // A text message is as tall as the core predicts. A message with images is measured here.
    var height = images.isEmpty && text != nil ? (result.text?.size.height ?? 0) + spec.insetHeight : y + spacing.xxs
    var avatarFrame = CGRect.zero
    if !own {
      // The avatar sits at the bottom of the row, over the row's padding and its own margin.
      height = max(height, Self.avatarSize + 3 * spacing.xxs)
      avatarFrame = CGRect(x: spacing.md, y: height - 2 * spacing.xxs - Self.avatarSize, width: Self.avatarSize, height: Self.avatarSize)
    }
    let imageFrames = Self.imageFrames(count: images.count, gap: spacing.xxs).map { direction.place($0, in: result.imagesFrame.width) }
    return ShadowListUtilsChatBubbleLayout(
      width: width, height: height, maximumBubbleWidth: columnLimit, avatarFrame: direction.place(avatarFrame), name: result.name,
      nameFrame: direction.place(result.nameFrame), bubbleFrame: direction.place(result.bubbleFrame), text: result.text,
      textFrame: direction.place(result.textFrame), caption: result.caption, captionFrame: direction.place(result.captionFrame),
      failed: result.failed, failedFrame: direction.place(result.failedFrame), imagesFrame: direction.place(result.imagesFrame),
      imageFrames: imageFrames)
  }

  private struct Frames {
    var name: ShadowListKitTextLayout?
    var nameFrame = CGRect.zero
    var bubbleFrame = CGRect.zero
    var text: ShadowListKitTextLayout?
    var textFrame = CGRect.zero
    var caption: ShadowListKitTextLayout?
    var captionFrame = CGRect.zero
    var failed: ShadowListKitTextLayout?
    var failedFrame = CGRect.zero
    var imagesFrame = CGRect.zero
  }

  /*
   * One image shows large. Several show as a grid of square tiles in two columns.
   */
  private static func imagesSize(count: Int, gap: CGFloat) -> CGSize {
    guard count > 1 else { return singleImageSize }
    let lines = CGFloat((count + 1) / 2)
    return CGSize(width: singleImageSize.width, height: lines * gridTileSize + (lines - 1) * gap)
  }

  private static func imageFrames(count: Int, gap: CGFloat) -> [CGRect] {
    guard count > 1 else { return count == 1 ? [CGRect(origin: .zero, size: singleImageSize)] : [] }
    return (0..<count).map { index in
      CGRect(
        x: CGFloat(index % 2) * (gridTileSize + gap), y: CGFloat(index / 2) * (gridTileSize + gap), width: gridTileSize, height: gridTileSize)
    }
  }

  static let avatarSize: CGFloat = 30
  static let gridLimit = 4
  private static let singleImageSize = CGSize(width: 240, height: 320)
  private static let gridTileSize: CGFloat = 119
}

/*
 * imageFrames are in imagesFrame. maximumBubbleWidth is the widest a bubble gets at this width.
 */
public final class ShadowListUtilsChatBubbleLayout: ShadowListUtilsRowLayout {
  public let maximumBubbleWidth: CGFloat
  public let avatarFrame: CGRect
  public let name: ShadowListKitTextLayout?
  public let nameFrame: CGRect
  public let bubbleFrame: CGRect
  public let text: ShadowListKitTextLayout?
  public let textFrame: CGRect
  public let caption: ShadowListKitTextLayout?
  public let captionFrame: CGRect
  public let failed: ShadowListKitTextLayout?
  public let failedFrame: CGRect
  public let imagesFrame: CGRect
  public let imageFrames: [CGRect]

  init(
    width: CGFloat, height: CGFloat, maximumBubbleWidth: CGFloat, avatarFrame: CGRect, name: ShadowListKitTextLayout?,
    nameFrame: CGRect, bubbleFrame: CGRect, text: ShadowListKitTextLayout?, textFrame: CGRect, caption: ShadowListKitTextLayout?,
    captionFrame: CGRect, failed: ShadowListKitTextLayout?, failedFrame: CGRect, imagesFrame: CGRect, imageFrames: [CGRect]
  ) {
    self.maximumBubbleWidth = maximumBubbleWidth
    self.avatarFrame = avatarFrame
    self.name = name
    self.nameFrame = nameFrame
    self.bubbleFrame = bubbleFrame
    self.text = text
    self.textFrame = textFrame
    self.caption = caption
    self.captionFrame = captionFrame
    self.failed = failed
    self.failedFrame = failedFrame
    self.imagesFrame = imagesFrame
    self.imageFrames = imageFrames
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsChatBubbleView: ShadowListUtilsRowView, UIGestureRecognizerDelegate {
  private let avatar = ShadowListUtilsAvatar(size: ShadowListUtilsChatBubble.avatarSize, theme: .system)
  private let name = ShadowListKitTextView(frame: .zero)
  private let bubble = UIImageView()
  private let text = ShadowListKitTextView(frame: .zero)
  private let caption = ShadowListKitTextView(frame: .zero)
  private let failed = ShadowListKitTextView(frame: .zero)
  private let retry = UIView()
  private let imageBlock = UIView()
  private var images: [ShadowListUtilsImageView] = []

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    let tap = UITapGestureRecognizer(target: self, action: #selector(retryTapped))
    tap.delegate = self
    retry.addGestureRecognizer(tap)
    for _ in 0..<ShadowListUtilsChatBubble.gridLimit {
      let view = ShadowListUtilsImageView()
      imageBlock.addSubview(view)
      images.append(view)
    }
    [avatar, name, bubble, text, caption, failed, retry, imageBlock].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsChatBubble else { return }
    let item = row.item
    let colors = row.theme.colors
    let own = item.isOwn
    restingBackgroundColor = colors.background
    avatar.isHidden = own || !row.showsAuthor
    if !avatar.isHidden {
      avatar.theme = row.theme
      avatar.show(name: item.author.name, imageUrl: item.author.avatarUrl, color: item.author.avatarColor)
    }
    // The small corner is on the side the message is on, which a right to left layout swaps.
    bubble.image = ShadowListUtilsBubbleImage.image(
      radius: row.theme.radius.lg + Self.radiusExtra, tailOnRight: own != ShadowListUtilsPlatform.isRightToLeft)
    bubble.tintColor = own ? colors.blue : colors.elevated2
    let sending = item.status == .sending
    [bubble, text, caption].forEach { $0.alpha = sending ? Self.sendingOpacity : 1 }

    let shown = Array(item.images.prefix(ShadowListUtilsChatBubble.gridLimit))
    imageBlock.isHidden = shown.isEmpty
    for (index, view) in images.enumerated() {
      view.isHidden = index >= shown.count
      view.cornerRadius = shown.count > 1 ? row.theme.radius.sm : row.theme.radius.lg
      view.placeholderColor = colors.elevated2
      view.url = index < shown.count ? shown[index] : nil
    }

    let parts: [String?] =
      [own ? nil : item.author.name, item.text] + shown.map { _ in row.labels.image }
      + [row.shownCaption, item.status == .failed ? row.labels.failed : nil]
    accessibilityLabel = parts.compactMap { $0 }.filter { !$0.isEmpty }.joined(separator: ", ")
    accessibilityHint = sending ? row.labels.sending : nil
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsChatBubble, let layout = layout as? ShadowListUtilsChatBubbleLayout else { return }
    let colors = row.theme.colors
    let own = row.item.isOwn
    avatar.frame = layout.avatarFrame
    name.isHidden = layout.name == nil
    name.show(layout.name, colors.secondaryLabel)
    name.frame = layout.nameFrame
    bubble.isHidden = layout.text == nil
    bubble.frame = layout.bubbleFrame
    text.isHidden = layout.text == nil
    text.show(layout.text, own ? colors.onAccent : colors.label)
    text.frame = layout.textFrame
    caption.isHidden = layout.caption == nil
    caption.show(layout.caption, layout.text != nil && own ? colors.onAccent.withAlphaComponent(Self.ownCaptionOpacity) : colors.secondaryLabel)
    caption.frame = layout.captionFrame
    failed.isHidden = layout.failed == nil
    failed.show(layout.failed, colors.red)
    failed.frame = layout.failedFrame
    retry.isHidden = layout.failed == nil
    retry.frame = layout.failedFrame.insetBy(dx: -Self.retrySlop, dy: -Self.retrySlop)
    imageBlock.frame = layout.imagesFrame
    for (view, frame) in zip(images, layout.imageFrames) {
      view.frame = frame
    }
  }

  private var chatList: ShadowListUtilsChatList? {
    enclosingList as? ShadowListUtilsChatList
  }

  /*
   * Whether a point in the row's coordinates is on the bubble, the part of a message a hold is
   * for.
   */
  func bubbleContains(_ point: CGPoint) -> Bool {
    !bubble.isHidden && bubble.frame.contains(point)
  }

  @objc private func retryTapped() {
    guard let row = row as? ShadowListUtilsChatBubble else { return }
    chatList?.onRetryMessage?(row.item)
  }

  /*
   * The failed line takes a tap only in a list that listens for it. Otherwise the tap is the
   * row's.
   */
  public override func gestureRecognizerShouldBegin(_ gestureRecognizer: UIGestureRecognizer) -> Bool {
    if gestureRecognizer.view === retry {
      return chatList?.onRetryMessage != nil
    }
    return super.gestureRecognizerShouldBegin(gestureRecognizer)
  }

  private static let radiusExtra: CGFloat = 2
  private static let sendingOpacity: CGFloat = 0.6
  private static let ownCaptionOpacity: CGFloat = 0.65
  private static let retrySlop: CGFloat = 8
}

/*
 * Stretchable bubble images, drawn once per radius and side: every corner rounded by the
 * radius, and the bottom corner on the sender's side by the tail radius. Main thread only.
 */
enum ShadowListUtilsBubbleImage {
  private static var images: [String: UIImage] = [:]

  static func image(radius: CGFloat, tailOnRight: Bool) -> UIImage {
    let key = "\(radius)#\(tailOnRight)"
    if let image = images[key] {
      return image
    }
    let side = 2 * radius + 4
    let rect = CGRect(x: 0, y: 0, width: side, height: side)
    let bottomRight = tailOnRight ? tailRadius : radius
    let bottomLeft = tailOnRight ? radius : tailRadius
    let drawn = UIGraphicsImageRenderer(size: rect.size).image { _ in
      let path = UIBezierPath()
      path.move(to: CGPoint(x: radius, y: 0))
      path.addLine(to: CGPoint(x: rect.maxX - radius, y: 0))
      path.addArc(withCenter: CGPoint(x: rect.maxX - radius, y: radius), radius: radius, startAngle: -.pi / 2, endAngle: 0, clockwise: true)
      path.addLine(to: CGPoint(x: rect.maxX, y: rect.maxY - bottomRight))
      path.addArc(
        withCenter: CGPoint(x: rect.maxX - bottomRight, y: rect.maxY - bottomRight), radius: bottomRight, startAngle: 0,
        endAngle: .pi / 2, clockwise: true)
      path.addLine(to: CGPoint(x: bottomLeft, y: rect.maxY))
      path.addArc(
        withCenter: CGPoint(x: bottomLeft, y: rect.maxY - bottomLeft), radius: bottomLeft, startAngle: .pi / 2, endAngle: .pi,
        clockwise: true)
      path.addLine(to: CGPoint(x: 0, y: radius))
      path.addArc(withCenter: CGPoint(x: radius, y: radius), radius: radius, startAngle: .pi, endAngle: 1.5 * .pi, clockwise: true)
      UIColor.black.setFill()
      path.fill()
    }
    let inset = radius + 1
    let image = drawn.resizableImage(withCapInsets: UIEdgeInsets(top: inset, left: inset, bottom: inset, right: inset))
      .withRenderingMode(.alwaysTemplate)
    images[key] = image
    return image
  }

  private static let tailRadius: CGFloat = 5
}
