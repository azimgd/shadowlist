import UIKit

/* UIButton owns tracking, cancellation and highlighting; the row supplies native child layout. */
final class ShadowListUtilsGroupedRowContent: UIButton {
  var theme: ShadowListUtilsTheme = .system { didSet { refresh() } }
  override var isHighlighted: Bool { didSet { refresh() } }
  private func refresh() { backgroundColor = isHighlighted ? theme.colors.fill : theme.colors.groupedCell }
}
