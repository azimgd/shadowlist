import ShadowListUtils
import UIKit

/*
 * The app controls expansion; the list owns only the visible rows of this file tree.
 */
final class TreeScreen: UIViewController {
  let list = ShadowListUtilsTreeList(theme: AppTheme.theme)
  private let roots = TripFileFixtures.roots
  private var expanded: Set<String> = Set(TripFileFixtures.roots.map(\.key))

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Trip Files"
    list.frame = view.bounds
    list.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    view.addSubview(list)
    list.expandedKeys = expanded
    list.onExpandedChange = { [weak self] keys in
      guard let self else { return }
      expanded = keys
      list.expandedKeys = keys
    }
    list.onPressItem = { [weak self] item in
      let alert = UIAlertController(title: item.name, message: "\(item.key)", preferredStyle: .alert)
      alert.addAction(UIAlertAction(title: "Close", style: .cancel))
      self?.present(alert, animated: true)
    }
    list.items = roots
    navigationItem.rightBarButtonItem = UIBarButtonItem(image: UIImage(systemName: "ellipsis"), menu: UIMenu(children: [
      UIAction(title: "Expand All", image: UIImage(systemName: "chevron.down.2")) { [weak self] _ in
        guard let self else { return }
        expanded = TripFileFixtures.folderKeys(roots)
        list.expandedKeys = expanded
      },
      UIAction(title: "Collapse All", image: UIImage(systemName: "chevron.up.2")) { [weak self] _ in
        self?.expanded = []
        self?.list.expandedKeys = []
      },
    ]))
  }
}
