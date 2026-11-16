import ShadowListKit
import UIKit

/*
 * The data source and delegate of a ShadowListUtilsList. It reads the list's rows, takes sizes
 * from the layout cache and hands the delegate calls a template needs back to the list.
 */
class ShadowListUtilsListAdapter: NSObject, ShadowListKitListViewDataSource, ShadowListKitListViewDelegate {
  weak var list: ShadowListUtilsList?
  private var registered = Set<String>()

  /*
   * The header rows of a list with sections. A plain list has none.
   */
  var headerRows: [ShadowListUtilsRow] { [] }

  // MARK: - Cells

  /*
   * A cell showing the row. The list learns a reuse identifier the first time a row with it
   * arrives.
   */
  func cell(for row: ShadowListUtilsRow, in listView: ShadowListKitListView) -> ShadowListKitListCell {
    let identifier = row.reuseIdentifier
    if registered.insert(identifier).inserted {
      listView.register(row.viewClass, forCellReuseIdentifier: identifier)
    }
    let cell = listView.dequeueReusableCell(withIdentifier: identifier)
    if let view = cell as? ShadowListUtilsRowView {
      show(row, in: view)
    }
    return cell
  }

  /*
   * Configure a row view with the row and its layout from the cache, once the list knows its
   * cross size.
   */
  func show(_ row: ShadowListUtilsRow, in view: ShadowListUtilsRowView) {
    guard let list else { return }
    let crossSize = list.itemCrossSize
    view.configure(row, layout: crossSize > 0 ? list.layoutCache.layout(row, width: crossSize) : nil, crossSize: crossSize > 0 ? crossSize : -1)
  }

  /*
   * The row's size along the list from its layout: the height, or the width in a horizontal
   * list.
   */
  func size(of row: ShadowListUtilsRow, crossSize: CGFloat, in listView: ShadowListKitListView) -> CGFloat {
    guard let list else { return 0 }
    list.itemCrossSize = crossSize
    let layout = list.layoutCache.layout(row, width: crossSize)
    return listView.isHorizontal ? layout.width : layout.height + (listView.showsSeparators && !(row is ShadowListUtilsSectionHeader) ? ShadowListUtilsTheme.hairline : 0)
  }

  // MARK: - Data

  func numberOfItems(in listView: ShadowListKitListView) -> Int {
    list?.rows.count ?? 0
  }

  func listView(_ listView: ShadowListKitListView, keyForItemAt index: Int) -> String {
    list?.rows[index].key ?? ""
  }

  func listView(_ listView: ShadowListKitListView, cellForItemAt index: Int) -> ShadowListKitListCell {
    guard let list else { return ShadowListKitListCell(reuseIdentifier: nil) }
    return cell(for: list.rows[index], in: listView)
  }

  func listView(_ listView: ShadowListKitListView, sizeForItemAt index: Int, crossSize: CGFloat) -> CGFloat {
    guard let list else { return 0 }
    return size(of: list.rows[index], crossSize: crossSize, in: listView)
  }

  func listView(_ listView: ShadowListKitListView, contentVersionForItemAt index: Int) -> Int {
    guard let list else { return 0 }
    return list.contentVersion(of: list.rows[index])
  }

  // MARK: - Events

  func listViewDidReachStart(_ listView: ShadowListKitListView) {
    list?.onStartReached?()
  }

  func listViewDidReachEnd(_ listView: ShadowListKitListView) {
    list?.onEndReached?()
  }

  func listViewDidBeginRefreshing(_ listView: ShadowListKitListView) {
    list?.onRefresh?()
  }

  func listViewDidScroll(_ listView: ShadowListKitListView) {
    list?.didScroll()
  }

  // MARK: - Cells leaving

  func listView(_ listView: ShadowListKitListView, didEndDisplaying cell: ShadowListKitListCell, at index: Int) {
    guard let list else { return }
    (cell as? ShadowListUtilsRowView)?.didEndDisplaying(in: list)
  }

  // MARK: - Selection

  func listView(_ listView: ShadowListKitListView, didSelectItemAt index: Int) {
    list?.didSelectItem(at: index)
  }

  func listView(_ listView: ShadowListKitListView, didDeselectItemAt index: Int) {
    list?.didDeselectItem(at: index)
  }

  // MARK: - Drag

  func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, to destinationIndex: Int) {
    list?.moveItem(from: sourceIndex, to: destinationIndex)
  }

  func listView(_ listView: ShadowListKitListView, moveItemAt sourceIndex: Int, toListId: String, destinationIndex: Int, destinationKey: String) {
    list?.moveItem(from: sourceIndex, toListId: toListId, destinationIndex: destinationIndex, destinationKey: destinationKey)
  }

  // MARK: - Swipe actions

  func listView(_ listView: ShadowListKitListView, leadingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    list?.leadingSwipeActions(at: index)
  }

  func listView(_ listView: ShadowListKitListView, trailingSwipeActionsForItemAt index: Int) -> ShadowListKitSwipeActionsConfiguration? {
    list?.trailingSwipeActions(at: index)
  }
}

/*
 * The adapter of a list whose items have menus. The kit adds its menu interaction only for a
 * delegate that answers this call.
 */
final class ShadowListUtilsMenuListAdapter: ShadowListUtilsListAdapter {
  func listView(_ listView: ShadowListKitListView, contextMenuForItemAt index: Int) -> UIMenu? {
    list?.contextMenu(at: index)
  }
}

/*
 * The adapter of a list that hears of a hold on an item. The kit adds its long press only for a
 * delegate that answers this call.
 */
final class ShadowListUtilsLongPressListAdapter: ShadowListUtilsListAdapter {
  func listView(_ listView: ShadowListKitListView, didLongPressItemAt index: Int) {
    list?.didLongPressItem(at: index)
  }
}

/*
 * One section of a list with sections: its key, the row of its header and how many of the
 * list's rows are its items.
 */
struct ShadowListUtilsListSection {
  let key: String
  let header: ShadowListUtilsRow
  let count: Int
}

/*
 * The adapter of a list with sections. The list's rows are the items of every section in
 * order. The kit groups items in sections only for a data source that answers these calls.
 */
final class ShadowListUtilsSectionListAdapter: ShadowListUtilsListAdapter {
  var sections: [ShadowListUtilsListSection] = []
  var indexTitles: [String]?

  override var headerRows: [ShadowListUtilsRow] { sections.map(\.header) }

  func numberOfSections(in listView: ShadowListKitListView) -> Int {
    sections.count
  }

  func listView(_ listView: ShadowListKitListView, numberOfItemsInSection section: Int) -> Int {
    sections[section].count
  }

  func listView(_ listView: ShadowListKitListView, keyForSection section: Int) -> String {
    sections[section].key
  }

  /*
   * A title, the header row's key here, gives the section its header. The cell below shows it.
   */
  func listView(_ listView: ShadowListKitListView, titleForHeaderInSection section: Int) -> String? {
    sections[section].header.key
  }

  func listView(_ listView: ShadowListKitListView, cellForHeaderInSection section: Int) -> ShadowListKitListCell {
    cell(for: sections[section].header, in: listView)
  }

  func listView(_ listView: ShadowListKitListView, sizeForHeaderInSection section: Int, crossSize: CGFloat) -> CGFloat {
    size(of: sections[section].header, crossSize: crossSize, in: listView)
  }

  func sectionIndexTitles(for listView: ShadowListKitListView) -> [String]? {
    indexTitles
  }
}
