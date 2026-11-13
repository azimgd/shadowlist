import ShadowListKit
import UIKit

/*
 * Draws the percentage and the app's selected state. A press does not change either on its own.
 */
public final class ShadowListUtilsPollRowView: ShadowListUtilsRowView {
  private let selection = UIView()
  private let icon = UIImageView()
  private let initial = ShadowListKitTextView(frame: .zero)
  private let label = ShadowListKitTextView(frame: .zero)
  private let percent = ShadowListKitTextView(frame: .zero)
  private let bar = UIView()
  private let barFill = UIView()

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    icon.contentMode = .scaleAspectFit
    [selection, icon, initial, label, percent, bar, barFill].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsPollRow else { return }
    let colors = row.theme.colors
    restingBackgroundColor = colors.background
    pressedBackgroundColor = row.interactive ? colors.fill : nil
    selection.backgroundColor = row.isLeading ? colors.accentSoft : colors.elevated
    selection.layer.borderColor = colors.accent.cgColor
    selection.layer.borderWidth = row.isSelected ? 2 : 0
    icon.image = row.item.icon?.withRenderingMode(.alwaysTemplate)
    icon.tintColor = row.isLeading ? colors.accent : colors.label
    icon.isHidden = row.item.icon == nil
    initial.isHidden = row.item.icon != nil
    bar.backgroundColor = colors.fill
    barFill.backgroundColor = row.isLeading ? colors.accent : colors.secondaryLabel
    accessibilityLabel = row.accessibilityLabel
    accessibilityTraits = row.interactive ? .button : []
    if row.isSelected { accessibilityTraits.insert(.selected) }
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsPollRow, let layout = layout as? ShadowListUtilsPollRowLayout else { return }
    selection.frame = layout.selectionFrame
    selection.layer.cornerRadius = layout.selectionFrame.width / 2
    icon.frame = layout.selectionFrame.insetBy(dx: 11, dy: 11)
    initial.show(layout.initial, row.isLeading ? row.theme.colors.accent : row.theme.colors.label)
    initial.frame = layout.initialFrame
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
