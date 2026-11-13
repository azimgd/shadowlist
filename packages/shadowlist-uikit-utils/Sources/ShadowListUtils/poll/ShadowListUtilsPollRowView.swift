import ShadowListKit
import UIKit

/*
 * Draws the percentage and the app's selected state. A press does not change either on its own.
 */
public final class ShadowListUtilsPollRowView: ShadowListUtilsRowView {
  private let selection = UIView()
  private let check = UIImageView(image: UIImage(systemName: "checkmark"))
  private let label = ShadowListKitTextView(frame: .zero)
  private let percent = ShadowListKitTextView(frame: .zero)
  private let bar = UIView()
  private let barFill = UIView()

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    check.contentMode = .scaleAspectFit
    [selection, check, label, percent, bar, barFill].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsPollRow else { return }
    let colors = row.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = row.interactive ? colors.fill : nil
    selection.backgroundColor = row.isSelected ? colors.accent : .clear
    selection.layer.borderColor = (row.isSelected ? colors.accent : colors.tertiaryLabel).cgColor
    selection.layer.borderWidth = 1
    check.tintColor = colors.onAccent
    check.isHidden = !row.isSelected
    bar.backgroundColor = colors.fill
    barFill.backgroundColor = row.isLeading ? colors.accent : colors.tertiaryLabel
    accessibilityLabel = row.accessibilityLabel
    accessibilityTraits = row.interactive ? .button : []
    if row.isSelected { accessibilityTraits.insert(.selected) }
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsPollRow, let layout = layout as? ShadowListUtilsPollRowLayout else { return }
    selection.frame = layout.selectionFrame
    selection.layer.cornerRadius = layout.selectionFrame.width / 2
    check.frame = layout.selectionFrame.insetBy(dx: 5, dy: 5)
    label.show(layout.label, row.theme.colors.label)
    label.frame = layout.labelFrame
    percent.show(layout.percent, row.isLeading ? row.theme.colors.accent : row.theme.colors.secondaryLabel)
    percent.frame = layout.percentFrame
    bar.frame = layout.barFrame
    bar.layer.cornerRadius = layout.barFrame.height / 2
    barFill.frame = layout.barFillFrame
    barFill.layer.cornerRadius = layout.barFillFrame.height / 2
  }
}
