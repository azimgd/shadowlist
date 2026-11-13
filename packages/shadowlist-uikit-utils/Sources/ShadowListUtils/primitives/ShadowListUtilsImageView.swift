import UIKit

/*
 * Shows a remote image, aspect filled and clipped, over a placeholder color. Setting url loads
 * it through ShadowListUtilsImageLoader.shared at the size the view is laid out at. A view that
 * gets a new url before the previous one arrives drops the previous result.
 */
public final class ShadowListUtilsImageView: UIImageView {
  public var url: String? {
    didSet {
      guard url != oldValue else { return }
      ShadowListUtilsImageLoader.shared.cancel(self)
      image = nil
      requestedUrl = nil
      loadIfNeeded()
    }
  }

  public var cornerRadius: CGFloat = 0 {
    didSet { layer.cornerRadius = cornerRadius }
  }

  /*
   * Shown behind the image and while it loads. Templates pass the theme's elevated2.
   * UIImageView keeps its previous background when the new color looks the same right now, like
   * a color of the light theme after a color that follows the appearance. Clearing it first
   * makes the new color take.
   */
  public var placeholderColor: UIColor? = ShadowListUtilsTheme.system.colors.elevated2 {
    didSet {
      backgroundColor = nil
      backgroundColor = placeholderColor
    }
  }

  private var requestedUrl: String?

  public init() {
    super.init(frame: .zero)
    contentMode = .scaleAspectFill
    clipsToBounds = true
    backgroundColor = placeholderColor
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    loadIfNeeded()
  }

  /*
   * Ask the loader once per url, as soon as the view has a size to decode for.
   */
  private func loadIfNeeded() {
    guard let url, requestedUrl != url, bounds.width > 0, bounds.height > 0 else { return }
    requestedUrl = url
    ShadowListUtilsImageLoader.shared.load(url, into: self)
  }
}
