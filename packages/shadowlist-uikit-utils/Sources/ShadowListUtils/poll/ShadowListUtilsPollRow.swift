import ShadowListKit
import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * An option's label, share and selection. The core computes its percentage with the same
 * rounding as every other template host. An icon replaces the first Unicode scalar of the label.
 */
public struct ShadowListUtilsPollRow: ShadowListUtilsRow {
  public let item: ShadowListUtilsPollOption
  public let totalVotes: Double
  public let isLeading: Bool
  public let isSelected: Bool
  public let interactive: Bool
  public let theme: ShadowListUtilsTheme
  public let percent: Int
  public let contentVersion: Int
  let accessibilityLabel: String

  public init(
    item: ShadowListUtilsPollOption, totalVotes: Double, isLeading: Bool = false, isSelected: Bool = false,
    interactive: Bool = false, theme: ShadowListUtilsTheme, labels: ShadowListUtilsPollLabels = ShadowListUtilsPollLabels()
  ) {
    self.item = item
    self.totalVotes = totalVotes
    self.isLeading = isLeading
    self.isSelected = isSelected
    self.interactive = interactive
    self.theme = theme
    percent = ShadowListUtilsCore.pollPercent(forVotes: item.votes, totalVotes: totalVotes)
    accessibilityLabel = labels.option(item.label, percent)
    var hasher = Hasher()
    hasher.combine(item)
    hasher.combine(totalVotes)
    hasher.combine(isLeading)
    hasher.combine(isSelected)
    hasher.combine(interactive)
    hasher.combine(accessibilityLabel)
    contentVersion = hasher.finalize()
  }

  public var key: String { item.key }
  public var viewClass: ShadowListUtilsRowView.Type { ShadowListUtilsPollRowView.self }

  public func layout(width: CGFloat) -> ShadowListUtilsRowLayout {
    let spacing = theme.spacing
    let typography = theme.typography
    let direction = ShadowListUtilsDirection(width: width)
    let top: CGFloat = 14
    let selectionFrame = CGRect(x: spacing.lg, y: top, width: Self.selectionSize, height: Self.selectionSize)
    let initial = typography.headline.layout(
      item.label.unicodeScalars.first.map { String($0).uppercased() } ?? "", width: Self.selectionSize,
      maxLines: 1, scaled: true, alignment: .center)
    let initialFrame = CGRect(x: selectionFrame.minX, y: top + (Self.selectionSize - initial.size.height) / 2,
                              width: Self.selectionSize, height: initial.size.height)
    let bodyX = selectionFrame.maxX + 14
    let bodyWidth = max(1, width - bodyX - spacing.lg)
    let percentText = typography.subhead.with(isLeading ? .semibold : .regular).layout("\(percent)%", width: bodyWidth / 3, maxLines: 1)
    let percentFrame = CGRect(CGPoint(x: width - spacing.lg - percentText.size.width, y: top), percentText)
    let label = typography.headline.layout(item.label, width: max(1, bodyWidth - percentText.size.width - spacing.sm))
    let labelFrame = CGRect(CGPoint(x: bodyX, y: top), label)
    let barFrame = CGRect(x: bodyX, y: max(labelFrame.maxY, percentFrame.maxY) + spacing.sm, width: bodyWidth, height: Self.barHeight)
    let fillWidth = bodyWidth * CGFloat(max(0, min(100, percent))) / 100
    let barFillFrame = CGRect(x: barFrame.minX, y: barFrame.minY, width: fillWidth, height: barFrame.height)
    return ShadowListUtilsPollRowLayout(
      width: width, height: max(selectionFrame.maxY, barFrame.maxY) + spacing.md,
      selectionFrame: direction.place(selectionFrame), initial: initial, initialFrame: direction.place(initialFrame), label: label, labelFrame: direction.place(labelFrame),
      percent: percentText, percentFrame: direction.place(percentFrame), barFrame: direction.place(barFrame),
      barFillFrame: direction.place(barFillFrame))
  }

  static let selectionSize: CGFloat = 44
  private static let barHeight: CGFloat = 6
}
