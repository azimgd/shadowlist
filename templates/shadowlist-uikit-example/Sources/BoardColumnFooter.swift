import ShadowListUtils
import UIKit

final class BoardColumnFooter: UIView {
  private let button: ShadowListUtilsDashedButton
  init(button: ShadowListUtilsDashedButton) {
    self.button = button
    super.init(frame: .zero)
    addSubview(button)
  }
  required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  override func sizeThatFits(_ size: CGSize) -> CGSize {
    let inset = button.theme.spacing.md
    let fitted = button.sizeThatFits(CGSize(width: size.width - 2 * inset, height: size.height))
    return CGSize(width: size.width, height: fitted.height + 2 * inset)
  }
  override func layoutSubviews() {
    super.layoutSubviews()
    button.frame = bounds.insetBy(dx: button.theme.spacing.md, dy: button.theme.spacing.md)
  }
}
