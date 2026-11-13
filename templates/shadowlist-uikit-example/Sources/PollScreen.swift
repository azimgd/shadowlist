import ShadowListUtils
import UIKit

/*
 * The application applies each choice and sends the updated model back to the list.
 */
final class PollScreen: UIViewController {
  let list = ShadowListUtilsPollList(theme: AppTheme.theme)
  private var poll = ShadowListUtilsPollData(question: "Where should we travel next?", options: [
    ShadowListUtilsPollOption(key: "lisbon", label: "Lisbon", votes: 12),
    ShadowListUtilsPollOption(key: "kyoto", label: "Kyoto", votes: 9),
    ShadowListUtilsPollOption(key: "reykjavik", label: "Reykjavík", votes: 7),
    ShadowListUtilsPollOption(key: "oaxaca", label: "Oaxaca", votes: 5),
  ])

  override func viewDidLoad() {
    super.viewDidLoad()
    title = "Poll"
    view.backgroundColor = AppTheme.colors.background
    list.frame = view.bounds
    list.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    list.onPressItem = { [weak self] option in self?.choose(option) }
    view.addSubview(list)
    list.poll = poll
  }

  private func choose(_ option: ShadowListUtilsPollOption) {
    guard poll.selectedKey != option.key else { return }
    for index in poll.options.indices {
      if poll.options[index].key == poll.selectedKey { poll.options[index].votes -= 1 }
      if poll.options[index].key == option.key { poll.options[index].votes += 1 }
    }
    poll.selectedKey = option.key
    list.setPoll(poll, animated: true)
  }
}
