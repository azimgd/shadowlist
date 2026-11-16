import ShadowListKit
import UIKit

public final class ShadowListUtilsTreeRowView: ShadowListUtilsRowView {
  private let name = ShadowListKitTextView(frame: .zero)
  private let chevron = UIImageView()
  private let icon = UIImageView()
  private let count = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    chevron.contentMode = .scaleAspectFit
    icon.contentMode = .center
    [name, chevron, icon, count].forEach(addSubview)
  }

  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsTreeRow else { return }
    restingBackgroundColor = row.theme.colors.background
    pressedBackgroundColor = row.interactive ? row.theme.colors.elevated : nil
    chevron.image = ShadowListUtilsChevronIcon.image(size: 14, strokeWidth: 1.75)
    chevron.transform = CGAffineTransform(rotationAngle: row.isExpanded ? .pi / 2 : 0)
    chevron.tintColor = row.theme.colors.tertiaryLabel
    chevron.isHidden = !row.hasChildren
    let folder = (row.item.kind ?? (row.item.children == nil ? .file : .folder)) == .folder
    icon.image = row.icon ?? (folder ? ShadowListUtilsFolderIcon.image(size: 20) : ShadowListUtilsDocIcon.image(size: 18))
    icon.tintColor = folder ? row.theme.colors.accent : row.theme.colors.secondaryLabel
    accessibilityLabel = row.item.name
    accessibilityHint = row.hasChildren ? (row.isExpanded ? row.labels.collapse : row.labels.expand) : nil
    accessibilityValue = row.hasChildren ? (row.isExpanded ? row.labels.collapse : row.labels.expand) : nil
    accessibilityTraits = row.interactive ? .button : []
  }

  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsTreeRow, let layout = layout as? ShadowListUtilsTreeRowLayout else { return }
    name.show(layout.name, row.theme.colors.label)
    name.frame = layout.nameFrame
    count.show(layout.count, row.theme.colors.tertiaryLabel)
    count.frame = layout.countFrame
    count.isHidden = layout.count == nil
    chevron.frame = layout.chevronFrame
    icon.frame = layout.iconFrame
  }
}
