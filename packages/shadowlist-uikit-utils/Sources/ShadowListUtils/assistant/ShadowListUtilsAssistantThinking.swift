import ShadowListKit
import UIKit

/* Local disclosure state; changing reasoning never mutates an application turn. */
public final class ShadowListUtilsAssistantThinking: UIView {
  public var thinking: String { didSet { changedLayout() } }
  public var thinkingMs: Double = 0 { didSet { changedLayout() } }
  public var active: Bool { didSet { changedLayout() } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { changedLayout() } }
  public let theme: ShadowListUtilsTheme
  var expanded = false
  var onLayoutChange: (() -> Void)?
  private let disclosure: ShadowListUtilsAssistantDisclosure
  private var motionObserver: NSObjectProtocol?

  public init(thinking: String, active: Bool, theme: ShadowListUtilsTheme = .system) {
    self.thinking = thinking; self.active = active; self.theme = theme
    disclosure = ShadowListUtilsAssistantDisclosure(theme: theme)
    super.init(frame: .zero)
    addSubview(disclosure)
    disclosure.onPress = { [weak self] in self?.toggle() }
    motionObserver = NotificationCenter.default.addObserver(forName: UIAccessibility.reduceMotionStatusDidChangeNotification, object: nil, queue: .main) { [weak self] _ in self?.updatePulse() }
  }
  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  deinit { if let motionObserver { NotificationCenter.default.removeObserver(motionObserver) } }
  struct Layout {
    let title: String
    let text: String
    let body: ShadowListUtilsAssistantDisclosureTextLayout
    let size: CGSize
  }
  static func title(thinkingMs: Double, active: Bool, labels: ShadowListUtilsAssistantLabels) -> String {
    if active { return labels.thinking }
    let duration = (thinkingMs / 1000).rounded()
    let seconds = !duration.isFinite || duration < 1 ? 1 : duration >= Double(Int.max) ? Int.max : Int(duration)
    return labels.thoughtFor(seconds)
  }
  static func layout(thinking: String, title: String, expanded: Bool, width: CGFloat, theme: ShadowListUtilsTheme) -> Layout {
    let text = expanded ? thinking : String(decoding: Array(thinking.utf16.suffix(160)), as: UTF16.self)
    let body = ShadowListUtilsAssistantDisclosureTextLayout(text: text, width: max(1, width - theme.spacing.md), style: theme.typography.footnote, theme: theme, expanded: expanded)
    return Layout(title: title, text: text, body: body, size: CGSize(width: width, height: 28 + theme.spacing.xs + body.size.height + theme.spacing.sm))
  }
  private func makeLayout(width: CGFloat) -> Layout {
    Self.layout(thinking: thinking, title: Self.title(thinkingMs: thinkingMs, active: active, labels: labels), expanded: expanded, width: width, theme: theme)
  }
  public override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: makeLayout(width: max(1, bounds.width)).size.height) }
  public override func sizeThatFits(_ size: CGSize) -> CGSize { makeLayout(width: size.width).size }
  public override func layoutSubviews() {
    super.layoutSubviews()
    let layout = makeLayout(width: bounds.width)
    disclosure.frame = bounds
    disclosure.show(title: layout.title, text: layout.text, layout: layout.body, expanded: expanded)
    updatePulse()
  }
  public override func didMoveToWindow() { super.didMoveToWindow(); updatePulse() }
  func toggle() { expanded.toggle(); changedLayout(); onLayoutChange?() }
  private func changedLayout() { setNeedsLayout(); invalidateIntrinsicContentSize() }
  private func updatePulse() {
    let layer = disclosure.header.titleLabel?.layer
    guard layer?.animation(forKey: "pulse") == nil || !active || window == nil || UIAccessibility.isReduceMotionEnabled else { return }
    layer?.removeAnimation(forKey: "pulse")
    guard active, window != nil, !UIAccessibility.isReduceMotionEnabled else { return }
    let pulse = CABasicAnimation(keyPath: "opacity")
    pulse.fromValue = 0.25; pulse.toValue = 1; pulse.duration = 0.6; pulse.autoreverses = true; pulse.repeatCount = .infinity
    layer?.add(pulse, forKey: "pulse")
  }
}
