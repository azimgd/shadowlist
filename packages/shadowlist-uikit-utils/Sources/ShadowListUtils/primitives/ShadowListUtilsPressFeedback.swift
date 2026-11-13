import UIKit

/* Native controls own their pressed state. UIKit highlights even when a theme requests ripple. */
enum ShadowListUtilsPressFeedback {
  static func alpha(pressed: Bool, disabled: Bool = false, pressedOpacity: CGFloat = 0.55, disabledOpacity: CGFloat = 0.4) -> CGFloat {
    disabled ? disabledOpacity : pressed ? pressedOpacity : 1
  }
}
