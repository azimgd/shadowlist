import ShadowListKit
import UIKit

/* Shared measured disclosure surface for standalone components and transcript sections. */
final class ShadowListUtilsAssistantDisclosure: UIView {
  let header = UIButton(type: .system)
  let body = UITextView(frame: .zero, textContainer: nil)
  private let theme: ShadowListUtilsTheme
  private let progress = UIActivityIndicatorView(style: .medium)
  var status: ShadowListUtilsAssistantToolStatus? { didSet { updateStatus() } }
  var onPress: (() -> Void)?
  init(theme: ShadowListUtilsTheme) {
    self.theme = theme
    super.init(frame: .zero)
    addSubview(header); addSubview(body)
    progress.isAccessibilityElement = false; header.addSubview(progress)
    header.contentHorizontalAlignment = .leading
    header.setTitleColor(theme.colors.secondaryLabel, for: .normal)
    header.titleLabel?.font = .systemFont(ofSize: theme.typography.subhead.fontSize * ShadowListUtilsPlatform.textScale)
    header.addAction(UIAction { [weak self] _ in self?.onPress?() }, for: .touchUpInside)
    body.isAccessibilityElement = true
  }
  @available(*, unavailable)
  required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  func show(title: String, text: String, layout: ShadowListUtilsAssistantDisclosureTextLayout, expanded: Bool) {
    header.setTitle(title + (expanded ? " ⌄" : " ›"), for: .normal)
    header.accessibilityLabel = title
    header.accessibilityValue = expanded ? "Expanded" : "Collapsed"
    layout.apply(to: body)
    body.accessibilityLabel = text
    body.frame = CGRect(x: theme.spacing.md, y: 28 + theme.spacing.xs, width: layout.size.width, height: layout.size.height)
    header.frame = CGRect(x: theme.spacing.md, y: 0, width: max(0, bounds.width - 2 * theme.spacing.md), height: 28)
    updateStatus()
  }
  private func updateStatus() {
    let symbol: String?
    switch status {
    case .running: symbol = "circle.dotted"; progress.color = theme.colors.secondaryLabel; progress.startAnimating()
    case .done: symbol = "checkmark"; progress.stopAnimating()
    case .failed: symbol = "xmark"; progress.stopAnimating()
    case .stopped: symbol = "stop.fill"; progress.stopAnimating()
    case nil: symbol = nil; progress.stopAnimating()
    }
    header.setImage(symbol.flatMap { UIImage(systemName: $0) }, for: .normal)
    switch status {
    case .running: header.tintColor = .clear
    case .done: header.tintColor = theme.colors.green
    case .failed: header.tintColor = theme.colors.red
    case .stopped: header.tintColor = theme.colors.tertiaryLabel
    case nil: header.tintColor = theme.colors.secondaryLabel
    }
    header.layoutIfNeeded()
    if let image = header.imageView { progress.frame = image.frame }
  }
}
