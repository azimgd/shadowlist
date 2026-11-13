import ShadowListUtils
import UIKit

/*
 * The app controls expansion; the list owns only the visible rows of this file tree.
 */
final class TreeScreen: UIViewController {
  let list = ShadowListUtilsTreeList(theme: AppTheme.theme)
  private var expanded: Set<String> = ["src"]

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Files"
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
    list.items = [
      ShadowListUtilsTreeNode(key: "src", name: "src", children: [
        ShadowListUtilsTreeNode(key: "components", name: "components", children: [
          ShadowListUtilsTreeNode(key: "avatar", name: "Avatar.tsx"),
          ShadowListUtilsTreeNode(key: "header", name: "Header.tsx"),
        ]),
        ShadowListUtilsTreeNode(key: "screens", name: "screens", children: [
          ShadowListUtilsTreeNode(key: "feed", name: "FeedScreen.tsx"),
          ShadowListUtilsTreeNode(key: "chat", name: "ChatScreen.tsx"),
        ]),
        ShadowListUtilsTreeNode(key: "app", name: "App.tsx"),
      ]),
      ShadowListUtilsTreeNode(key: "assets", name: "assets", children: []),
      ShadowListUtilsTreeNode(key: "readme", name: "README.md"),
    ]
  }
}
