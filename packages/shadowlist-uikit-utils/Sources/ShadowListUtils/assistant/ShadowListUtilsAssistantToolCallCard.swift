import ShadowListKit
import UIKit

/* A tool call with application-owned status and local disclosure state. */
public final class ShadowListUtilsAssistantToolCallCard: UIView {
  public var call: ShadowListUtilsAssistantToolCall { didSet { if oldValue.key != call.key { expanded = false }; changedLayout() } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { changedLayout() } }
  public var grouped = false { didSet { applyStyle() } }
  public var separated = false { didSet { separator.isHidden = !separated } }
  public let theme: ShadowListUtilsTheme
  var expanded = false
  var onLayoutChange: (() -> Void)?
  private let disclosure: ShadowListUtilsAssistantDisclosure
  private let separator = UIView()

  public init(call: ShadowListUtilsAssistantToolCall, theme: ShadowListUtilsTheme = .system) {
    self.call = call; self.theme = theme
    disclosure = ShadowListUtilsAssistantDisclosure(theme: theme)
    super.init(frame: .zero)
    addSubview(disclosure); addSubview(separator)
    separator.backgroundColor = theme.colors.separator; separator.isHidden = true
    disclosure.onPress = { [weak self] in self?.toggle() }
    applyStyle()
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  struct Layout {
    let title: String
    let text: String
    let body: ShadowListUtilsAssistantDisclosureTextLayout
    let size: CGSize
  }
  static func layout(call: ShadowListUtilsAssistantToolCall, title: String, status: String, labels: ShadowListUtilsAssistantLabels, expanded: Bool, width: CGFloat, theme: ShadowListUtilsTheme) -> Layout {
    let output: String?
    switch call.status { case let .done(value): output = value; case let .failed(value): output = value; default: output = nil }
    let text = expanded ? "\(labels.toolInput)\n\(call.input)" + (output.flatMap { $0.isEmpty ? nil : $0 }.map { "\n\n\(labels.toolOutput)\n\($0)" } ?? "") : ((output?.isEmpty == false ? output : nil) ?? status)
    let body = ShadowListUtilsAssistantDisclosureTextLayout(text: text, width: max(1, width - 2 * theme.spacing.md), style: theme.typography.footnote, theme: theme, expanded: expanded, monospaced: expanded)
    return Layout(title: title, text: text, body: body, size: CGSize(width: width, height: 32 + body.size.height + 2 * theme.spacing.sm))
  }
  private func makeLayout(width: CGFloat) -> Layout {
    let status = labels.toolStatus(call.status)
    return Self.layout(call: call, title: "\(call.name), \(status)", status: status, labels: labels, expanded: expanded, width: width, theme: theme)
  }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: makeLayout(width: max(1, bounds.width)).size.height) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { makeLayout(width: size.width).size }
  public override func layoutSubviews() {
    super.layoutSubviews()
    let layout = makeLayout(width: bounds.width)
    disclosure.frame = bounds
    disclosure.status = call.status
    disclosure.show(title: layout.title, text: layout.text, layout: layout.body, expanded: expanded)
    let inset = 2 * theme.spacing.md + 20
    separator.frame = ShadowListUtilsDirection(width: bounds.width).place(CGRect(x: inset, y: 0, width: max(0, bounds.width - inset), height: ShadowListUtilsTheme.hairline))
  }
  func toggle() { expanded.toggle(); changedLayout(); onLayoutChange?() }
  private func changedLayout() { setNeedsLayout(); invalidateIntrinsicContentSize() }
  private func applyStyle() {
    backgroundColor = grouped ? .clear : theme.colors.elevated
    layer.cornerRadius = grouped ? 0 : theme.radius.md
  }
}
