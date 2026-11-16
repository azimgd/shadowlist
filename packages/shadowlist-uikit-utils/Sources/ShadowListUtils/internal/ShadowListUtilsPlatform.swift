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
   * A length rounded to the pixel grid, rounded down to it and rounded up to it.
   */
  static func roundToPixel(_ value: CGFloat) -> CGFloat {
    let scale = displayScale
    return (value * scale).rounded() / scale
  }

  static func floorToPixel(_ value: CGFloat) -> CGFloat {
    let scale = displayScale
    return (value * scale).rounded(.down) / scale
  }

  static func ceilToPixel(_ value: CGFloat) -> CGFloat {
    let scale = displayScale
    return (value * scale - pixelEpsilon).rounded(.up) / scale
  }

  /*
   * Where a button's one-line title sits in its content: as wide as its glyphs on whole pixels
   * and centered from a floored origin, the way React Native places a centered text. A title
   * wider than the content keeps the button's own rectangle.
   */
  static func centeredTitleRect(_ rect: CGRect, title: NSAttributedString?, in content: CGRect) -> CGRect {
    guard let width = title.map({ ceilToPixel($0.size().width) }), width <= content.width else { return rect }
    return CGRect(x: content.minX + floorToPixel((content.width - width) / 2), y: rect.minY, width: width, height: rect.height)
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
