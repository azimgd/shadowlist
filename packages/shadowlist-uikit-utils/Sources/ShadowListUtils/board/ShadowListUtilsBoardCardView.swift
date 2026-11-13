import ShadowListKit
import UIKit

public final class ShadowListUtilsBoardCardView: ShadowListUtilsRowView {
  private let card = UIView()
  private let accent = UIView()
  private let title = ShadowListKitTextView(frame: .zero)
  private let detail = ShadowListKitTextView(frame: .zero)

  public required init(reuseIdentifier: String?) {
    super.init(reuseIdentifier: reuseIdentifier)
    isAccessibilityElement = true
    addSubview(card)
    [accent, title, detail].forEach(card.addSubview)
  }
  public override func fill(_ row: ShadowListUtilsRow) {
    guard let row = row as? ShadowListUtilsBoardCard else { return }
    restingBackgroundColor = .clear
    pressedBackgroundColor = nil
    card.backgroundColor = row.theme.colors.background
    card.layer.cornerRadius = row.theme.radius.md
    accent.backgroundColor = row.color
    accent.layer.cornerRadius = row.theme.spacing.xs / 2
    accessibilityLabel = row.accessibilityLabel ?? [row.title, row.label, row.detail].compactMap { $0 }.joined(separator: ", ")
  }
  public override func apply(_ row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    guard let row = row as? ShadowListUtilsBoardCard, let layout = layout as? ShadowListUtilsBoardCardLayout else { return }
    card.frame = layout.card
    accent.frame = layout.accent.offsetBy(dx: -layout.card.minX, dy: -layout.card.minY)
    title.show(layout.title, row.theme.colors.label)
    title.frame = layout.titleFrame.offsetBy(dx: -layout.card.minX, dy: -layout.card.minY)
    detail.show(layout.detail, row.theme.colors.secondaryLabel)
    detail.frame = layout.detailFrame.offsetBy(dx: -layout.card.minX, dy: -layout.card.minY)
  }
}
