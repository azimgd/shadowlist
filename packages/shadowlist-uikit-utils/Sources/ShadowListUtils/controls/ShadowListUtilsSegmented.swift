import UIKit

public final class ShadowListUtilsSegmented: UIView {
  public var options: [ShadowListUtilsSegmentedOption] { didSet { rebuild() } }
  public var value: String { didSet { refresh() } }
  public var onChange: ((String) -> Void)?
  public var size: ShadowListUtilsSegmentedSize = .regular { didSet { refresh() } }
  public let theme: ShadowListUtilsTheme
  private let track = UIView()
  private let stack = UIStackView()
  private var buttons: [ShadowListUtilsSegmentedButton] = []

  public init(options: [ShadowListUtilsSegmentedOption], value: String, theme: ShadowListUtilsTheme = .system) {
    self.options = options; self.value = value; self.theme = theme
    super.init(frame: .zero)
    isAccessibilityElement = false; accessibilityContainerType = .semanticGroup
    stack.axis = .horizontal; stack.spacing = 0
    addSubview(track); track.addSubview(stack); rebuild()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  public override var intrinsicContentSize: CGSize {
    CGSize(width: size == .compact ? buttons.reduce(4) { $0 + $1.intrinsicContentSize.width } : UIView.noIntrinsicMetric,
      height: size == .compact ? 34 : 36)
  }
  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: self.size == .compact ? intrinsicContentSize.width : size.width, height: intrinsicContentSize.height)
  }
  public override func layoutSubviews() {
    super.layoutSubviews()
    let width = size == .compact ? min(bounds.width, intrinsicContentSize.width) : bounds.width
    let rightToLeft = effectiveUserInterfaceLayoutDirection == .rightToLeft
    track.frame = CGRect(x: rightToLeft ? bounds.width - width : 0, y: 0, width: width, height: intrinsicContentSize.height)
    stack.semanticContentAttribute = rightToLeft ? .forceRightToLeft : .forceLeftToRight
    stack.frame = track.bounds.insetBy(dx: 2, dy: 2)
  }
  public override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
      ShadowListUtilsPlatform.resolve(category: traitCollection.preferredContentSizeCategory, window: window); refresh()
    }
  }
  private func rebuild() {
    var available = buttons
    buttons = options.map { option in
      let button: ShadowListUtilsSegmentedButton
      if let index = available.firstIndex(where: { $0.option.id == option.id }) { button = available.remove(at: index) }
      else { button = ShadowListUtilsSegmentedButton(option: option, theme: theme) }
      button.option = option
      button.onPress = { [weak self, weak button] in guard let button else { return }; self?.onChange?(button.option.id) }
      return button
    }
    stack.replaceArrangedSubviews(buttons)
    refresh()
  }
  private func refresh() {
    let compact = size == .compact
    track.backgroundColor = compact ? theme.colors.fill : theme.colors.elevated2
    track.layer.cornerRadius = compact ? 8 : 9
    stack.distribution = compact ? .fill : .fillEqually
    let selected = options.firstIndex { $0.id == value }
    for (index, button) in buttons.enumerated() {
      button.compact = compact; button.isSelected = selected == index
      button.divider.isHidden = index == 0 || selected == index || selected == index - 1
      button.refresh()
    }
    invalidateIntrinsicContentSize(); setNeedsLayout()
    superview?.invalidateIntrinsicContentSize(); superview?.setNeedsLayout()
  }
}
