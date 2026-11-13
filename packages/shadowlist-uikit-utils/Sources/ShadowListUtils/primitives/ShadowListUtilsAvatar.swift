import os
import ShadowListKit
import UIKit

// A separate module with SwiftPM, and part of this one with CocoaPods.
#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * A colored circle with the initials of a name. The color is the one given or the palette color
 * of the name, the same on every platform. An image covers the initials when it loads. The
 * initials keep their size whatever the text size setting. The avatar is hidden from
 * accessibility: the row it sits in reads the name.
 */
public final class ShadowListUtilsAvatar: UIView {
  public var theme: ShadowListUtilsTheme {
    didSet {
      if theme != oldValue {
        show(name: name, imageUrl: imageUrl, color: color)
      }
    }
  }

  public let size: CGFloat
  private let initials = ShadowListKitTextView(frame: .zero)
  private let image = ShadowListUtilsImageView()
  private var name = ""
  private var imageUrl: String?
  private var color: UIColor?

  public init(size: CGFloat = 40, theme: ShadowListUtilsTheme) {
    self.size = size
    self.theme = theme
    super.init(frame: CGRect(x: 0, y: 0, width: size, height: size))
    layer.cornerRadius = size / 2
    clipsToBounds = true
    isAccessibilityElement = false
    image.frame = bounds
    image.placeholderColor = .clear
    image.isHidden = true
    addSubview(initials)
    addSubview(image)
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  /*
   * Show a person. An empty name shows the color alone.
   */
  public func show(name: String, imageUrl: String? = nil, color: UIColor? = nil) {
    self.name = name
    self.imageUrl = imageUrl
    self.color = color
    backgroundColor = color ?? theme.colors.avatarColor(for: name)
    let layout = Self.initialsLayout(name, size: size)
    initials.show(layout, theme.colors.label)
    initials.frame = CGRect(
      x: ShadowListUtilsPlatform.roundToPixel((size - layout.size.width) / 2),
      y: ShadowListUtilsPlatform.roundToPixel((size - layout.size.height) / 2), width: layout.size.width, height: layout.size.height)
    image.isHidden = imageUrl == nil
    image.url = imageUrl
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: self.size, height: self.size)
  }

  // MARK: - Initials

  private static let layouts = OSAllocatedUnfairLock(uncheckedState: [String: ShadowListKitTextLayout]())

  /*
   * The initials of a name typeset for an avatar of a size, once per pair.
   */
  private static func initialsLayout(_ name: String, size: CGFloat) -> ShadowListKitTextLayout {
    let text = ShadowListUtilsCore.avatarInitials(forName: name).uppercased()
    let key = "\(text)#\(size)"
    return layouts.withLockUnchecked { layouts in
      if let layout = layouts[key] {
        return layout
      }
      let metrics = ShadowListUtilsCore.avatarInitialsMetrics(forSize: size)
      let style = ShadowListUtilsTextStyle(fontSize: metrics.fontSize, fontWeight: .semibold, lineHeight: metrics.lineHeight, letterSpacing: 0)
      let layout = style.layout(text, width: size, maxLines: 1, scaled: false)
      layouts[key] = layout
      return layout
    }
  }
}

extension ShadowListUtilsThemeColors {
  /*
   * The name's slot of the palette, the same on every platform, or the fill color for an empty
   * palette.
   */
  func avatarColor(for name: String) -> UIColor {
    guard !avatarPalette.isEmpty else { return fill }
    let index = ShadowListUtilsCore.avatarColorIndex(forName: name, paletteSize: avatarPalette.count)
    return avatarPalette.indices.contains(index) ? avatarPalette[index] : fill
  }
}
