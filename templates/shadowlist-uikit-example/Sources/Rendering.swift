import ShadowListKit
import ShadowListUtils

/*
 * -SLTextAsync 1 draws the rows' text on a background queue. -SLImages 0 loads no images.
 */
enum Rendering {
  static let asyncText = UserDefaults.standard.string(forKey: "SLTextAsync") == "1"

  static func apply() {
    if asyncText {
      ShadowListKitTextView.appearance().displaysAsynchronously = true
    }
    if !Settings.images {
      ShadowListUtilsImageLoader.shared = NoImageLoader()
    }
  }
}
