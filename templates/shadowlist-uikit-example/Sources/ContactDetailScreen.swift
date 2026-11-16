import ShadowListKit
import ShadowListUtils
import UIKit

final class ContactDetailScreen: UIViewController {
  private let contact: ShadowListUtilsContactItem

  init(_ contact: ShadowListUtilsContactItem) {
    self.contact = contact
    super.init(nibName: nil, bundle: nil)
    navigationItem.largeTitleDisplayMode = .never
  }

  required init?(coder: NSCoder) { fatalError() }

  override func viewDidLoad() {
    super.viewDidLoad()
    let theme = AppTheme.theme
    view.backgroundColor = theme.colors.groupedBackground
    let appearance = UINavigationBarAppearance()
    appearance.configureWithOpaqueBackground()
    appearance.backgroundColor = theme.colors.background
    appearance.shadowColor = .clear
    navigationItem.standardAppearance = appearance
    navigationItem.scrollEdgeAppearance = appearance
    let scroll = UIScrollView()
    scroll.contentInsetAdjustmentBehavior = .automatic
    scroll.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(scroll)
    let content = UIStackView()
    content.axis = .vertical
    content.spacing = theme.spacing.lg
    content.translatesAutoresizingMaskIntoConstraints = false
    scroll.addSubview(content)
    NSLayoutConstraint.activate([
      scroll.topAnchor.constraint(equalTo: view.topAnchor),
      scroll.bottomAnchor.constraint(equalTo: view.bottomAnchor),
      scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      content.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: theme.spacing.lg),
      content.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -theme.spacing.lg),
      content.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor),
      content.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor),
      content.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor),
    ])
    let hero = UIStackView()
    hero.axis = .vertical
    hero.alignment = .center
    hero.spacing = theme.spacing.md
    hero.isLayoutMarginsRelativeArrangement = true
    hero.layoutMargins = UIEdgeInsets(top: theme.spacing.lg, left: theme.spacing.lg, bottom: theme.spacing.lg, right: theme.spacing.lg)
    let avatar = ShadowListUtilsAvatar(size: 96, theme: theme)
    avatar.show(name: contact.name, imageUrl: contact.avatarUrl, color: contact.avatarColor)
    avatar.widthAnchor.constraint(equalToConstant: 96).isActive = true
    avatar.heightAnchor.constraint(equalToConstant: 96).isActive = true
    let name = UILabel()
    name.attributedText = theme.typography.title2.string(contact.name)
    name.textAlignment = .center
    name.numberOfLines = 0
    name.accessibilityTraits = .header
    name.heightAnchor.constraint(greaterThanOrEqualToConstant: theme.typography.title2.lineHeight).isActive = true
    hero.addArrangedSubview(avatar)
    hero.addArrangedSubview(name)
    content.addArrangedSubview(hero)
    if let subtitle = contact.subtitle, !subtitle.isEmpty {
      let row = UIStackView()
      row.axis = .vertical
      row.spacing = 2
      row.isLayoutMarginsRelativeArrangement = true
      row.layoutMargins = UIEdgeInsets(top: 11, left: theme.grouped.rowInset, bottom: 11, right: theme.grouped.rowInset)
      let label = UILabel()
      label.attributedText = theme.typography.subhead.string("mobile")
      let phone = UITextView()
      phone.backgroundColor = .clear
      phone.isEditable = false
      phone.isScrollEnabled = false
      phone.textContainerInset = .zero
      phone.textContainer.lineFragmentPadding = 0
      phone.attributedText = theme.typography.body.string(subtitle, theme.colors.accent)
      phone.heightAnchor.constraint(greaterThanOrEqualToConstant: theme.typography.body.lineHeight).isActive = true
      row.addArrangedSubview(label)
      row.addArrangedSubview(phone)
      content.addArrangedSubview(ShadowListUtilsGroupedCard(children: [row], theme: theme))
    }
  }
}
