import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * A poll with its question and total. The app owns votes and selectedKey; onPressItem requests
 * a vote and setPoll shows the app's answer. Custom headerView and footerView are kept.
 */
public final class ShadowListUtilsPollList: ShadowListUtilsList {
  public var poll: ShadowListUtilsPollData {
    get { storedPoll }
    set { setPoll(newValue, animated: false) }
  }

  public func setPoll(_ poll: ShadowListUtilsPollData, animated: Bool) {
    storedPoll = poll
    rebuildRows(animated: animated)
  }

  public var onPressItem: ((ShadowListUtilsPollOption) -> Void)? {
    didSet {
      allowsSelection = onPressItem != nil
      rebuildRows()
    }
  }

  public var rowForItem: ((ShadowListUtilsPollOption) -> ShadowListUtilsRow)? {
    didSet { reloadRows() }
  }

  public var labels = ShadowListUtilsPollLabels() {
    didSet { reloadRows() }
  }

  private var storedPoll = ShadowListUtilsPollData()
  private var pollHeader: UIView?
  private var pollFooter: UIView?

  public override init(theme: ShadowListUtilsTheme = .system) {
    super.init(theme: theme)
    allowsSelection = false
    stickyHeader = true
    stickyFooter = true
    pollHeader = ShadowListUtilsListHeader(title: storedPoll.question, theme: theme)
    pollFooter = ShadowListUtilsListFooter(text: labels.totalVotes(0), theme: theme)
    headerView = pollHeader
    footerView = pollFooter
    rebuildRows()
  }

  override func makeRows() -> [ShadowListUtilsRow]? {
    let tally = ShadowListUtilsCore.pollTally(forVotes: storedPoll.options.map { NSNumber(value: $0.votes) })
    if headerView === pollHeader {
      pollHeader = ShadowListUtilsListHeader(title: storedPoll.question, theme: theme)
      headerView = pollHeader
    }
    if footerView === pollFooter {
      pollFooter = ShadowListUtilsListFooter(text: labels.totalVotes(tally.totalVotes), theme: theme)
      footerView = pollFooter
    }
    return storedPoll.options.enumerated().map { index, item in
      rowForItem?(item) ?? ShadowListUtilsPollRow(
        item: item, totalVotes: tally.totalVotes, isLeading: index == tally.leadingIndex,
        isSelected: item.key == storedPoll.selectedKey, interactive: onPressItem != nil, theme: theme, labels: labels)
    }
  }

  override func didSelectItem(at index: Int) {
    if storedPoll.options.indices.contains(index) { onPressItem?(storedPoll.options[index]) }
    deselectItem(at: index, animated: true)
  }
}
