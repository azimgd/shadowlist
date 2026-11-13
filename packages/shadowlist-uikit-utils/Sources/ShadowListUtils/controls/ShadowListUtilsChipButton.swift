import UIKit

/* Native highlight ownership for the chip's main action; its clear action stays separate. */
final class ShadowListUtilsChipButton: UIButton {
  var onHighlight: ((Bool) -> Void)?
  override var isHighlighted: Bool { didSet { onHighlight?(isHighlighted) } }
}
