import UIKit

/*
 * UIButton owns tracking, cancellation and highlighting; the row supplies native child layout.
 */
final class ShadowListUtilsGroupedRowContent: UIButton {
  var theme: ShadowListUtilsTheme = .system { didSet { refresh() } }
  var passiveViews: [UIView] = []
  override var isHighlighted: Bool { didSet { refresh() } }
  override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
    guard let hit = super.hitTest(point, with: event) else { return nil }
    // Layout containers belong to the primary button; caller-supplied controls keep their taps.
    return passiveViews.contains(where: { $0 === hit }) ? self : hit
  }
  private func refresh() { backgroundColor = isHighlighted ? theme.colors.fill : theme.colors.groupedCell }
}
