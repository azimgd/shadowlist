import ShadowListUtils
import UIKit

/*
 * The app owns read state. Press an unread activity to mark it read; refresh prepends a new one.
 */
final class ActivityScreen: UIViewController {
  let list = ShadowListUtilsActivityList(theme: AppTheme.theme)
  private var items: [ShadowListUtilsActivityItem] = []
  private var fresh = 100
  private var positioned = false

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Activity"
    view.backgroundColor = AppTheme.colors.background
    list.frame = view.bounds
    list.contentInsetAdjustmentBehavior = .never
    list.alwaysBounceVertical = true
    let footer = UIView(frame: CGRect(x: 0, y: 0, width: 0, height: 2 * AppTheme.theme.spacing.xl))
    footer.backgroundColor = AppTheme.colors.background
    list.footerView = footer
    list.stickyFooter = true
    list.onPressItem = { [weak self] item in
      guard let self, let index = self.items.firstIndex(where: { $0.key == item.key }) else { return }
      self.items[index].read = true
      self.list.setItems(self.items, animated: true)
    }
    list.onRefresh = { [weak self] in
      guard let self else { return }
      self.items.insert(self.activity(self.fresh), at: 0)
      self.fresh += 1
      self.list.setItems(self.items, animated: true)
      self.list.isRefreshing = false
    }
    navigationItem.rightBarButtonItem = UIBarButtonItem(image: UIImage(systemName: "ellipsis"), menu: UIMenu(children: [
      UIAction(title: "Mark All Read", image: UIImage(systemName: "checkmark")) { [weak self] _ in
        guard let self else { return }
        for index in self.items.indices { self.items[index].read = true }
        self.list.setItems(self.items, animated: true)
      },
    ]))
    view.addSubview(list)
    items = (0..<100).map(activity)
    list.items = items
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    list.frame = CGRect(x: 0, y: view.safeAreaInsets.top, width: view.bounds.width, height: view.bounds.height - view.safeAreaInsets.top)
    guard !positioned, list.bounds.height > 0, items.count > 30 else { return }
    positioned = true
    list.scrollToItem(at: 30, viewPosition: 0, animated: false)
  }

  private func activity(_ index: Int) -> ShadowListUtilsActivityItem {
    let actions = ["started following you", "liked your trip photo", "tagged you in a check-in",
      "added you to a trip crew", "commented on your itinerary", "is boarding at gate 22"]
    return ShadowListUtilsActivityItem(
      key: "activity-\(index)",
      actor: ShadowListUtilsActivityActor(name: FixtureStrings.characterNames[index % FixtureStrings.characterNames.count]),
      action: actions[index % actions.count],
      text: String(FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.count].prefix(90)),
      createdAtMs: Date().timeIntervalSince1970 * 1000 - Double(index % 59 + 1) * 60000,
      read: true)
  }
}
