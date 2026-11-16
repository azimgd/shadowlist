import UIKit

/*
 * The system activity indicator, centered with padding around it. For a list's footerView while
 * more items load.
 */
public final class ShadowListUtilsSpinner: UIView {
  public var theme: ShadowListUtilsTheme {
    didSet { indicator.color = color ?? theme.colors.secondaryLabel }
  }

  public let size: CGFloat
  public let color: UIColor?
  private let indicator = UIActivityIndicatorView(style: .medium)

  public init(size: CGFloat = 20, color: UIColor? = nil, theme: ShadowListUtilsTheme) {
    self.size = size
    self.color = color
    self.theme = theme
    super.init(frame: .zero)
    indicator.color = color ?? theme.colors.secondaryLabel
    indicator.hidesWhenStopped = false
    indicator.transform = CGAffineTransform(scaleX: size / Self.indicatorSize, y: size / Self.indicatorSize)
    indicator.accessibilityLabel = Self.loadingLabel
    indicator.startAnimating()
    addSubview(indicator)
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  public override var intrinsicContentSize: CGSize {
    let side = size + 2 * theme.spacing.lg
    return CGSize(width: side, height: side)
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: size.width, height: self.size + 2 * theme.spacing.lg)
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    indicator.center = CGPoint(x: bounds.midX, y: bounds.midY)
  }

  /*
   * A spinner taken off screen by the list stops. It starts again when it comes back.
   */
  public override func didMoveToWindow() {
    super.didMoveToWindow()
    if window != nil {
      indicator.startAnimating()
    }
  }

  private static let indicatorSize: CGFloat = 20
  private static let loadingLabel = "Loading"
}
