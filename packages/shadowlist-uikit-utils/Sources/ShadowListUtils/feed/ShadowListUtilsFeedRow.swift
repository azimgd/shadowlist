import ShadowListKit
import UIKit

// A separate module with SwiftPM, and part of this one with CocoaPods.
#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * A post: an avatar, a line with the author's name, handle and the post's age, the text, and
 * one image or a strip of several. formatTime writes the age in place of the labels' words.
 * The age is worked out once, when the row is made.
 */
public struct ShadowListUtilsFeedRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsFeedItem
  public let theme: ShadowListUtilsTheme
  public let labels: ShadowListUtilsFeedLabels

  /*
   * The post's age in words, or nil for a post without a time and for a placeholder.
   */
  public let time: String?

  public let contentVersion: Int

  public init(
    item: ShadowListUtilsFeedItem,
    theme: ShadowListUtilsTheme,
    labels: ShadowListUtilsFeedLabels,
    formatTime: ((Double) -> String)? = nil
  ) {
    self.item = item
    self.theme = theme
    self.labels = labels
    let time = item.placeholder
      ? nil
      : item.createdAtMs.map { formatTime?($0) ?? labels.time.format(dateMs: $0, nowMs: ShadowListUtilsRelativeTimeLabels.nowMs) }
    self.time = time
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(time)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsFeedRowView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let subhead = theme.typography.subhead
    let direction = ShadowListUtilsDirection(width: width)
    let contentX = spacing.lg + Self.avatarSize + spacing.md
    let contentWidth = max(1, width - contentX - spacing.lg)
    let top = spacing.md

    let timeText = self.time.map { Self.timePrefix + $0 }
    let name: ShadowListKitTextLayout
    let handle: ShadowListKitTextLayout?
    let time: ShadowListKitTextLayout?
    let nameFrame: CGRect
    let handleFrame: CGRect
    let timeFrame: CGRect
    var headerBottom = top + subhead.scaledLineHeight()
    if ShadowListUtilsPlatform.isLargeText {
      /*
       * At a large text size each part takes the lines it needs, and a part with no room beside
       * the one before it starts a new line.
       */
      let lineWidth = max(1, contentWidth)
      name = subhead.with(.semibold).layout(item.author.name, width: lineWidth)
      handle = item.author.handle.map { subhead.layout($0, width: lineWidth) }
      time = timeText.map { subhead.layout($0, width: lineWidth) }
      var x: CGFloat = 0
      var y = top
      headerBottom = top
      func place(_ part: ShadowListKitTextLayout?) -> CGRect {
        guard let part else { return .zero }
        if x > 0, x + part.size.width > lineWidth {
          x = 0
          y = headerBottom + spacing.xs
        }
        let frame = CGRect(CGPoint(x: contentX + x, y: y), part)
        x += part.size.width + spacing.xs
        headerBottom = max(headerBottom, frame.maxY)
        return frame
      }
      nameFrame = place(name)
      handleFrame = place(handle)
      timeFrame = place(time)
    } else {
      // The age keeps its width. The name and then the handle take what is left of the line.
      time = timeText.map { subhead.layout($0, width: contentWidth, maxLines: 1) }
      var available = contentWidth - (time.map { $0.size.width + spacing.xs } ?? 0)
      name = subhead.with(.semibold).layout(item.author.name, width: max(1, available), maxLines: 1)
      available -= name.size.width + spacing.xs
      handle = item.author.handle.flatMap { available >= 1 ? subhead.layout($0, width: available, maxLines: 1) : nil }
      nameFrame = CGRect(CGPoint(x: contentX, y: top), name)
      handleFrame = handle.map { CGRect(CGPoint(x: nameFrame.maxX + spacing.xs, y: top), $0) } ?? .zero
      timeFrame = time.map { CGRect(CGPoint(x: (handle == nil ? nameFrame.maxX : handleFrame.maxX) + spacing.xs, y: top), $0) } ?? .zero
    }

    var bottom = headerBottom + spacing.xxs
    var text: ShadowListKitTextLayout?
    var textFrame = CGRect.zero
    if let body = item.text, !body.isEmpty {
      let typeset = subhead.layout(body, width: contentWidth)
      textFrame = CGRect(x: contentX, y: bottom, width: contentWidth, height: typeset.size.height)
      text = typeset
      bottom = textFrame.maxY + (item.images.isEmpty ? 0 : spacing.md)
    }

    var imageFrame = CGRect.zero
    var stripFrame = CGRect.zero
    var stripImageFrames: [CGRect] = []
    if item.images.count == 1, let image = item.images.first {
      /*
       * A single image keeps its own aspect within the limits the core sets, or takes the core's
       * fixed height when it has no size.
       */
      let frame = ShadowListUtilsCore.feedSingleImageFrame(forWidth: image.width ?? 0, height: image.height ?? 0)
      let height = frame.aspectRatio > 0 ? ShadowListUtilsPlatform.roundToPixel(contentWidth / frame.aspectRatio) : frame.height
      imageFrame = CGRect(x: contentX, y: bottom, width: contentWidth, height: height)
      bottom = imageFrame.maxY
    } else if item.images.count > 1 {
      // The strip starts and ends a gap outside the content, and every image has a gap on both sides.
      let size = Self.stripImageSize
      let step = size.width + 2 * spacing.xs
      let contentSize = CGFloat(item.images.count) * step
      stripFrame = CGRect(x: contentX - spacing.xs, y: bottom, width: contentWidth + 2 * spacing.xs, height: size.height)
      stripImageFrames = item.images.indices.map { index in
        direction.place(CGRect(x: spacing.xs + CGFloat(index) * step, y: 0, width: size.width, height: size.height), in: contentSize)
      }
      bottom = stripFrame.maxY
    }

    let avatarFrame = CGRect(x: spacing.lg, y: top, width: Self.avatarSize, height: Self.avatarSize)
    let height = max(bottom, avatarFrame.maxY) + spacing.md
    let hairline = ShadowListUtilsPlatform.hairline
    let separatorFrame = CGRect(x: theme.rowInset, y: height - hairline, width: max(0, width - theme.rowInset), height: hairline)
    return ShadowListUtilsFeedRowLayout(
      width: width, height: height, avatarFrame: direction.place(avatarFrame), name: name, nameFrame: direction.place(nameFrame),
      handle: handle, handleFrame: direction.place(handleFrame), time: time, timeFrame: direction.place(timeFrame), text: text,
      textFrame: direction.place(textFrame), imageFrame: direction.place(imageFrame), stripFrame: direction.place(stripFrame),
      stripImageFrames: stripImageFrames, separatorFrame: direction.place(separatorFrame))
  }

  static let avatarSize: CGFloat = 40

  /*
   * An image of a strip of several. Its height is the core's height of a single image without
   * a size.
   */
  static let stripImageSize = CGSize(width: 280, height: SHADOWLIST_UTILS_FEED_SINGLE_IMAGE_HEIGHT)

  /*
   * What goes between the handle and the age.
   */
  private static let timePrefix = "· "
}

/*
 * stripImageFrames are in the strip's content, one per image.
 */
public final class ShadowListUtilsFeedRowLayout: ShadowListUtilsRowLayout {
  public let avatarFrame: CGRect
  public let name: ShadowListKitTextLayout
  public let nameFrame: CGRect
  public let handle: ShadowListKitTextLayout?
  public let handleFrame: CGRect
  public let time: ShadowListKitTextLayout?
  public let timeFrame: CGRect
  public let text: ShadowListKitTextLayout?
  public let textFrame: CGRect
  public let imageFrame: CGRect
  public let stripFrame: CGRect
  public let stripImageFrames: [CGRect]
  public let separatorFrame: CGRect

  init(
    width: CGFloat, height: CGFloat, avatarFrame: CGRect, name: ShadowListKitTextLayout, nameFrame: CGRect,
    handle: ShadowListKitTextLayout?, handleFrame: CGRect, time: ShadowListKitTextLayout?, timeFrame: CGRect,
    text: ShadowListKitTextLayout?, textFrame: CGRect, imageFrame: CGRect, stripFrame: CGRect, stripImageFrames: [CGRect],
    separatorFrame: CGRect
  ) {
    self.avatarFrame = avatarFrame
    self.name = name
    self.nameFrame = nameFrame
    self.handle = handle
    self.handleFrame = handleFrame
    self.time = time
    self.timeFrame = timeFrame
    self.text = text
    self.textFrame = textFrame
    self.imageFrame = imageFrame
    self.stripFrame = stripFrame
    self.stripImageFrames = stripImageFrames
    self.separatorFrame = separatorFrame
    super.init(width: width, height: height)
  }
}

public final class ShadowListUtilsFeedRowView: ShadowListUtilsRowView, UIGestureRecognizerDelegate {
  private let avatar = ShadowListUtilsAvatar(size: ShadowListUtilsFeedRow.avatarSize, theme: .system)
  private let name = ShadowListKitTextView(frame: .zero)
  private let handle = ShadowListKitTextView(frame: .zero)
  private let time = ShadowListKitTextView(frame: .zero)
  private let text = ShadowListKitTextView(frame: .zero)
  private let nameBars = ShadowListUtilsRedactedTextView()
  private let handleBars = ShadowListUtilsRedactedTextView()
  private let textBars = ShadowListUtilsRedactedTextView()
  private let image = ShadowListUtilsImageView()
  private let strip = UIScrollView()
  private var stripImages: [ShadowListUtilsImageView] = []
  private let separator = UIView()

  /*
   * What a screen reader reads for the post's text. The images are elements of their own.
   */
  private lazy var textElement = UIAccessibilityElement(accessibilityContainer: self)

  /*
   * The key of the post the strip was last placed for. The strip goes back to its start only
   * for another post.
   */
  private var stripKey: String?

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    strip.showsHorizontalScrollIndicator = false
    strip.clipsToBounds = true
    strip.scrollsToTop = false
    image.isUserInteractionEnabled = true
    image.addGestureRecognizer(makeImageTap())
    [avatar, name, handle, time, text, nameBars, handleBars, textBars, image, strip, separator].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsFeedRow else { return }
    let item = row.item
    let colors = row.theme.colors
    let placeholder = item.placeholder
    restingBackgroundColor = colors.background
    separator.backgroundColor = colors.separator
    avatar.theme = row.theme
    if placeholder {
      avatar.show(name: "", color: colors.fill)
    } else {
      avatar.show(name: item.author.name, imageUrl: item.author.avatarUrl, color: item.author.avatarColor)
    }
    [name, handle, time, text].forEach { $0.isHidden = placeholder }
    [nameBars, handleBars, textBars].forEach { $0.isHidden = !placeholder }

    let single = item.images.count == 1
    image.isHidden = !single
    strip.isHidden = item.images.count < 2
    if single, let first = item.images.first {
      show(first, in: image, row: row)
    } else {
      image.url = nil
    }
    fillStrip(row)

    if placeholder {
      textElement.accessibilityLabel = row.labels.loading
    } else {
      textElement.accessibilityLabel = [item.author.name, item.author.handle, row.time, item.text].compactMap { $0 }.joined(separator: ", ")
    }
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsFeedRow, let layout = layout as? ShadowListUtilsFeedRowLayout else { return }
    let colors = row.theme.colors
    avatar.frame = layout.avatarFrame
    name.frame = layout.nameFrame
    handle.frame = layout.handleFrame
    time.frame = layout.timeFrame
    text.frame = layout.textFrame
    if row.item.placeholder {
      let subhead = row.theme.typography.subhead
      showBars(nameBars, layout.name, layout.nameFrame, subhead, colors.fill)
      showBars(handleBars, layout.handle, layout.handleFrame, subhead, colors.fill)
      showBars(textBars, layout.text, layout.textFrame, subhead, colors.fill)
    } else {
      name.show(layout.name, colors.label)
      handle.show(layout.handle, colors.secondaryLabel)
      time.show(layout.time, colors.secondaryLabel)
      text.show(layout.text, colors.label)
    }
    image.frame = layout.imageFrame
    placeStrip(row, layout)
    separator.frame = layout.separatorFrame

    // The text element covers the row down to its images, which keeps its activation point off them.
    let imagesTop = layout.imageFrame != .zero ? layout.imageFrame.minY : layout.stripFrame != .zero ? layout.stripFrame.minY : layout.height
    textElement.accessibilityFrameInContainerSpace = CGRect(x: 0, y: 0, width: layout.width, height: imagesTop)
  }

  private var feedList: ShadowListUtilsFeedList? {
    enclosingList as? ShadowListUtilsFeedList
  }

  /*
   * A placeholder shows the frame in the image's color alone.
   */
  private func show(_ image: ShadowListUtilsFeedImage, in view: ShadowListUtilsImageView, row: ShadowListUtilsFeedRow) {
    view.cornerRadius = row.theme.radius.lg
    view.placeholderColor = image.color ?? row.theme.colors.elevated2
    view.url = row.item.placeholder ? nil : image.url
    view.accessibilityLabel = image.alt ?? row.labels.image
  }

  /*
   * The strip of several images, each one in a view of its own.
   */
  private func fillStrip(_ row: ShadowListUtilsFeedRow) {
    let images = row.item.images.count > 1 ? row.item.images : []
    while stripImages.count < images.count {
      let view = ShadowListUtilsImageView()
      view.isUserInteractionEnabled = true
      view.addGestureRecognizer(makeImageTap())
      strip.addSubview(view)
      stripImages.append(view)
    }
    for (index, view) in stripImages.enumerated() {
      view.isHidden = index >= images.count
      if index < images.count {
        show(images[index], in: view, row: row)
      } else {
        view.url = nil
      }
    }
  }

  /*
   * Place the strip and its images. The strip opens at its start, which is its right end in a
   * layout that runs from right to left, and keeps its place while it shows the same post.
   */
  private func placeStrip(_ row: ShadowListUtilsFeedRow, _ layout: ShadowListUtilsFeedRowLayout) {
    strip.frame = layout.stripFrame
    for (view, frame) in zip(stripImages, layout.stripImageFrames) {
      view.frame = frame
    }
    let gap = row.theme.spacing.xs
    let contentWidth = CGFloat(layout.stripImageFrames.count) * (ShadowListUtilsFeedRow.stripImageSize.width + 2 * gap)
    strip.contentInset = UIEdgeInsets(top: 0, left: gap, bottom: 0, right: gap)
    strip.contentSize = CGSize(width: contentWidth, height: layout.stripFrame.height)
    guard stripKey != row.key else { return }
    stripKey = row.key
    let end = max(-gap, contentWidth + gap - layout.stripFrame.width)
    strip.contentOffset = CGPoint(x: ShadowListUtilsPlatform.isRightToLeft ? end : -gap, y: 0)
  }

  private func showBars(
    _ view: ShadowListUtilsRedactedTextView, _ layout: ShadowListKitTextLayout?, _ frame: CGRect, _ style: ShadowListUtilsTextStyle,
    _ color: ShadowListUtilsColor
  ) {
    view.frame = frame
    view.show(layout.map { ShadowListUtilsRedactedText.bars(for: $0, lineHeight: style.scaledLineHeight()) } ?? [], color)
  }

  // MARK: - Image presses

  private func makeImageTap() -> UITapGestureRecognizer {
    let tap = UITapGestureRecognizer(target: self, action: #selector(imageTapped(_:)))
    tap.delegate = self
    return tap
  }

  @objc private func imageTapped(_ recognizer: UITapGestureRecognizer) {
    guard let view = recognizer.view as? ShadowListUtilsImageView else { return }
    pressImage(at: view === image ? 0 : stripImages.firstIndex { $0 === view } ?? 0)
  }

  private func pressImage(at index: Int) {
    guard let row = row as? ShadowListUtilsFeedRow, !row.item.placeholder else { return }
    feedList?.onPressImage?(row.item, index)
  }

  /*
   * An image takes a tap only in a list that asks for image presses. Otherwise the tap is the
   * row's.
   */
  public override func gestureRecognizerShouldBegin(_ gestureRecognizer: UIGestureRecognizer) -> Bool {
    if gestureRecognizer is UITapGestureRecognizer, gestureRecognizer.view is ShadowListUtilsImageView {
      return feedList?.onPressImage != nil
    }
    return super.gestureRecognizerShouldBegin(gestureRecognizer)
  }

  // MARK: - Accessibility

  /*
   * The post's text reads as one element, a button in a list that selects. Each image is an
   * element with its alt text, and a button in a list that takes image presses.
   */
  public override var accessibilityElements: [Any]? {
    get {
      guard let row = row as? ShadowListUtilsFeedRow else { return nil }
      textElement.accessibilityTraits = accessibilityTraits
      textElement.accessibilityCustomActions = accessibilityCustomActions
      guard !row.item.placeholder else { return [textElement] }
      let pressable = feedList?.onPressImage != nil
      let images = row.item.images.count == 1 ? [image] : Array(stripImages.prefix(row.item.images.count > 1 ? row.item.images.count : 0))
      for view in images {
        view.isAccessibilityElement = true
        view.accessibilityTraits = pressable ? [.image, .button] : .image
      }
      return [textElement] + images
    }
    set { super.accessibilityElements = newValue }
  }
}
