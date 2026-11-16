import ShadowListUtils
import UIKit

final class HomeScreen: UIViewController {
  private let scroll = UIScrollView()
  private let sections = UIStackView()

  init() {
    super.init(nibName: nil, bundle: nil)
    title = "ShadowList"
    navigationItem.backButtonDisplayMode = .minimal
  }

  required init?(coder: NSCoder) { fatalError() }

  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = AppTheme.colors.groupedBackground
    navigationItem.largeTitleDisplayMode = .always
    let appearance = UINavigationBarAppearance()
    appearance.configureWithDefaultBackground()
    navigationItem.standardAppearance = appearance
    let edge = UINavigationBarAppearance()
    edge.configureWithTransparentBackground()
    navigationItem.scrollEdgeAppearance = edge
    scroll.translatesAutoresizingMaskIntoConstraints = false
    scroll.contentInsetAdjustmentBehavior = .automatic
    view.addSubview(scroll)
    sections.axis = .vertical
    sections.translatesAutoresizingMaskIntoConstraints = false
    scroll.addSubview(sections)
    NSLayoutConstraint.activate([
      scroll.topAnchor.constraint(equalTo: view.topAnchor),
      scroll.bottomAnchor.constraint(equalTo: view.bottomAnchor),
      scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      sections.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: AppTheme.theme.spacing.xxl),
      sections.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -AppTheme.theme.spacing.xxl * 2),
      sections.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor),
      sections.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor),
      sections.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor),
    ])
    for section in ExampleSection.all {
      let rows = section.examples.enumerated().map { index, example in
        let row = ShadowListUtilsGroupedRow(title: example.title, theme: AppTheme.theme)
        row.subtitle = example.summary
        row.subtitleLines = 2
        row.separated = index > 0
        row.chevron = true
        row.testID = example.route
        row.onPress = { [weak self] in
          self?.navigationController?.pushViewController(example.make(), animated: true)
        }
        return row
      }
      sections.addArrangedSubview(ShadowListUtilsGroupedSection(title: section.title, children: rows, theme: AppTheme.theme))
    }
    let engine = ShadowListUtilsGroupedRow(title: "List engine", theme: AppTheme.theme)
    engine.subtitle = Settings.engine.title
    engine.chevron = true
    engine.onPress = { [weak self, weak engine] in
      guard let self else { return }
      let sheet = UIAlertController(title: "List engine", message: nil, preferredStyle: .actionSheet)
      for option in Engine.allCases {
        sheet.addAction(UIAlertAction(title: option.title, style: .default) { _ in
          UserDefaults.standard.set(option.rawValue, forKey: "SLEngine")
          engine?.subtitle = option.title
        })
      }
      sheet.addAction(UIAlertAction(title: "Cancel", style: .cancel))
      sheet.popoverPresentationController?.sourceView = engine
      sheet.popoverPresentationController?.sourceRect = engine?.bounds ?? .zero
      self.present(sheet, animated: true)
    }
    sections.addArrangedSubview(ShadowListUtilsGroupedSection(children: [engine], theme: AppTheme.theme))
  }
}
