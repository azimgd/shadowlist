import UIKit

final class ShadowListUtilsTextInputView: UITextView {
  weak var editingOwner: ShadowListUtilsTextInput?
  override func unmarkText() { super.unmarkText(); editingOwner?.commitComposition() }
}
