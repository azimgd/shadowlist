import UIKit

/* A native composition boundary, without a delayed replacement or an extra editing store. */
final class ShadowListUtilsTextInputField: UITextField {
  weak var editingOwner: ShadowListUtilsTextInput?
  override func unmarkText() { super.unmarkText(); editingOwner?.commitComposition() }
}
