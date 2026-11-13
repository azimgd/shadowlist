import UIKit

/*
 * An action in the activity header. Its accessibility label defaults to the visible label.
 */
public struct ShadowListUtilsActivityHeaderAction {
  public var label: String
  public var onPress: () -> Void
  public var icon: UIImage?
  public var accessibilityLabel: String?

  public init(label: String, onPress: @escaping () -> Void, icon: UIImage? = nil, accessibilityLabel: String? = nil) {
    self.label = label
    self.onPress = onPress
    self.icon = icon
    self.accessibilityLabel = accessibilityLabel
  }
}
