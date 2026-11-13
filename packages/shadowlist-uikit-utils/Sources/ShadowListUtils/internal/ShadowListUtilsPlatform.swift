import os
import ShadowListKit
import UIKit

/*
 * The platform's color, font and view.
 */
typealias ShadowListUtilsColor = UIColor
typealias ShadowListUtilsFont = UIFont
typealias ShadowListUtilsView = UIView

/*
 * What a layout needs from the device: the pixel grid, the reader's text size, the layout
 * direction and the window's height. Only the main thread may ask UIKit for them. A list
 * resolves them there when it is made and when the text size changes, and a row's layout reads
 * the resolved values on any thread.
 */
enum ShadowListUtilsPlatform {
  private struct State {
    var resolved = false
    var displayScale: CGFloat = 0
    var textScale: CGFloat = 1
    var isRightToLeft = false
    var windowHeight: CGFloat = 0
    var textScaleOverride: CGFloat?
    var isRightToLeftOverride: Bool?
  }

  private static let state = OSAllocatedUnfairLock(initialState: State())

  /*
   * Read the device's values on the main thread. category is the reader's new text size when
   * the system reports a change, and window the one whose height a layout falls back to.
   */
  static func resolve(category: UIContentSizeCategory? = nil, window: UIWindow? = nil) {
    dispatchPrecondition(condition: .onQueue(.main))
    let screen = window?.screen ?? UIScreen.main
    let category = category ?? screen.traitCollection.preferredContentSizeCategory
    let textScale = Self.textScale(for: category)
    let isRightToLeft = UIView.userInterfaceLayoutDirection(for: .unspecified) == .rightToLeft
    let windowHeight = window?.bounds.height ?? screen.bounds.height
    let displayScale = screen.scale
    state.withLock { state in
      state.resolved = true
      state.displayScale = displayScale
      state.textScale = textScale
      state.isRightToLeft = isRightToLeft
      state.windowHeight = windowHeight
    }
  }

  /*
   * The resolved values. A layout that runs before any list was made resolves them itself on
   * the main thread, and off it reads the thread's own traits, which UIKit keeps for every
   * thread.
   */
  private static var current: State {
    let state = state.withLock { $0 }
    guard !state.resolved else { return state }
    if Thread.isMainThread {
      resolve()
      return self.state.withLock { $0 }
    }
    var fallback = state
    let traits = UITraitCollection.current
    fallback.displayScale = traits.displayScale
    fallback.textScale = textScale(for: traits.preferredContentSizeCategory)
    return fallback
  }

  private static func textScale(for category: UIContentSizeCategory) -> CGFloat {
    guard category != .unspecified else { return 1 }
    let traits = UITraitCollection(preferredContentSizeCategory: category)
    return UIFontMetrics.default.scaledValue(for: textScaleSample, compatibleWith: traits) / textScaleSample
  }

  // MARK: - Pixels

  /*
   * How many pixels a point is.
   */
  static var displayScale: CGFloat {
    let scale = current.displayScale
    return scale > 0 ? scale : fallbackDisplayScale
  }

  /*
   * The thickness of a separator: one device pixel.
   */
  static var hairline: CGFloat { 1 / displayScale }

  /*
   * A length rounded to the pixel grid, and rounded up to it.
   */
  static func roundToPixel(_ value: CGFloat) -> CGFloat {
    let scale = displayScale
    return (value * scale).rounded() / scale
  }

  static func ceilToPixel(_ value: CGFloat) -> CGFloat {
    let scale = displayScale
    return (value * scale - pixelEpsilon).rounded(.up) / scale
  }

  // MARK: - Text size

  /*
   * How much larger than its theme size text is set, by the reader's text size setting. 1 at
   * the system's default size.
   */
  static var textScale: CGFloat {
    let state = current
    return state.textScaleOverride ?? state.textScale
  }

  /*
   * Whether the reader's text size is large enough for rows to give their titles more lines.
   */
  static var isLargeText: Bool { textScale >= largeTextScale }

  /*
   * Stands in for the reader's text size in a test. nil follows the device again.
   */
  static var textScaleOverride: CGFloat? {
    get { state.withLock { $0.textScaleOverride } }
    set { state.withLock { $0.textScaleOverride = newValue } }
  }

  // MARK: - Direction

  /*
   * Whether the app lays out from right to left. A row then mirrors its frames.
   */
  static var isRightToLeft: Bool {
    let state = current
    return state.isRightToLeftOverride ?? state.isRightToLeft
  }

  /*
   * Stands in for the layout direction in a test. nil follows the device again.
   */
  static var isRightToLeftOverride: Bool? {
    get { state.withLock { $0.isRightToLeftOverride } }
    set { state.withLock { $0.isRightToLeftOverride = newValue } }
  }

  // MARK: - Window

  /*
   * The height of the window a list was last resolved in, or of the screen.
   */
  static var windowHeight: CGFloat { current.windowHeight }

  private static let textScaleSample: CGFloat = 1000
  private static let largeTextScale: CGFloat = 1.3
  private static let fallbackDisplayScale: CGFloat = 2
  private static let pixelEpsilon: CGFloat = 0.001
}

extension CGRect {
  /*
   * The frame of typeset text placed at origin.
   */
  init(_ origin: CGPoint, _ layout: ShadowListKitTextLayout) {
    self.init(origin: origin, size: layout.size)
  }

  /*
   * The frame at the same distance from the other side of a container of a width, for a layout
   * that runs from right to left.
   */
  func mirrored(in width: CGFloat) -> CGRect {
    CGRect(x: width - maxX, y: minY, width: self.width, height: height)
  }
}

/*
 * Places the frames of a finished layout: as they are, or mirrored in the row's width when the
 * app lays out from right to left. The frame of a part a row lacks stays empty.
 */
struct ShadowListUtilsDirection {
  let width: CGFloat
  let isRightToLeft: Bool

  init(width: CGFloat) {
    self.width = width
    isRightToLeft = ShadowListUtilsPlatform.isRightToLeft
  }

  func place(_ frame: CGRect) -> CGRect {
    isRightToLeft && frame != .zero ? frame.mirrored(in: width) : frame
  }

  /*
   * The same for frames inside a container of another width.
   */
  func place(_ frame: CGRect, in width: CGFloat) -> CGRect {
    isRightToLeft && frame != .zero ? frame.mirrored(in: width) : frame
  }
}

extension ShadowListKitTextView {
  func show(_ layout: ShadowListKitTextLayout?, _ color: ShadowListUtilsColor) {
    textColor = color
    textLayout = layout
  }
}

/*
 * The three icons the rows draw: the grip of a reorderable row, the chevron of a row that opens
 * something and the check of a selected row. Each fills its view, which is square.
 */
final class ShadowListUtilsIconView: ShadowListUtilsView {
  enum Kind {
    case grip
    case chevron
    case check
  }

  let kind: Kind

  var color: ShadowListUtilsColor = .clear {
    didSet { setNeedsDisplay() }
  }

  init(kind: Kind) {
    self.kind = kind
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
    let path = Self.path(kind, size: bounds.width, isRightToLeft: ShadowListUtilsPlatform.isRightToLeft)
    if kind == .grip {
      color.setFill()
      path.fill()
    } else {
      color.setStroke()
      path.stroke()
    }
  }

  /*
   * The icon's shape in a square of a size. The grip is filled and the others are stroked. A
   * chevron points left in a layout that runs from right to left.
   */
  static func path(_ kind: Kind, size: CGFloat, isRightToLeft: Bool = false) -> UIBezierPath {
    let path = UIBezierPath()
    switch kind {
    case .grip:
      // Three lines as wide as the icon, gripGapShare of its size apart.
      let step = gripStroke + size * gripGapShare
      let top = (size - (3 * gripStroke + 2 * size * gripGapShare)) / 2
      for line in 0..<3 {
        let frame = CGRect(x: 0, y: top + CGFloat(line) * step, width: size, height: gripStroke)
        path.append(UIBezierPath(roundedRect: frame, cornerRadius: gripStroke / 2))
      }
    case .chevron:
      // Two arms of chevronArmShare of the size, meeting at a right angle.
      let reach = size * chevronArmShare * halfDiagonal
      let side: CGFloat = isRightToLeft ? -1 : 1
      path.move(to: CGPoint(x: size / 2 - side * reach / 2, y: size / 2 - reach))
      path.addLine(to: CGPoint(x: size / 2 + side * reach / 2, y: size / 2))
      path.addLine(to: CGPoint(x: size / 2 - side * reach / 2, y: size / 2 + reach))
      path.lineWidth = chevronStroke
    case .check:
      // A short arm and a long one meeting at the bottom, the long one rising to the right.
      let short = size * checkShortShare * halfDiagonal
      let long = size * checkLongShare * halfDiagonal
      let corner = CGPoint(x: size / 2 + (short - long) / 2, y: size / 2 + (short + long) / 2 - size * checkLiftShare)
      path.move(to: CGPoint(x: corner.x - short, y: corner.y - short))
      path.addLine(to: corner)
      path.addLine(to: CGPoint(x: corner.x + long, y: corner.y - long))
      path.lineWidth = checkStroke
    }
    return path
  }

  private static let halfDiagonal: CGFloat = 0.70710677
  private static let gripStroke: CGFloat = 1.75
  private static let gripGapShare: CGFloat = 0.22
  private static let chevronStroke: CGFloat = 2
  private static let chevronArmShare: CGFloat = 0.42
  private static let checkStroke: CGFloat = 2.2
  private static let checkShortShare: CGFloat = 0.26
  private static let checkLongShare: CGFloat = 0.5
  private static let checkLiftShare: CGFloat = 0.06
}
