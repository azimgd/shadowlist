import ShadowListKit
import UIKit

/*
 * An image or file attachment with an optional application-owned remove action.
 */
public final class ShadowListUtilsAssistantAttachmentChip: UIView {
  public let attachment: ShadowListUtilsAssistantAttachment
  public var onRemove: ((String) -> Void)? { didSet { setNeedsLayout(); invalidateIntrinsicContentSize() } }
  public var labels = ShadowListUtilsAssistantLabels() { didSet { remove.accessibilityLabel = labels.removeAttachment(attachment.name) } }
  public let theme: ShadowListUtilsTheme
  private let image = ShadowListUtilsImageView()
  private let file = UIImageView(image: UIImage(systemName: "doc.text"))
  private let name = ShadowListKitTextView(frame: .zero)
  private let detail = ShadowListKitTextView(frame: .zero)
  private let remove = UIButton(type: .system)

  public init(attachment: ShadowListUtilsAssistantAttachment, theme: ShadowListUtilsTheme = .system, onRemove: ((String) -> Void)? = nil) {
    self.attachment = attachment
    self.theme = theme
    self.onRemove = onRemove
    super.init(frame: .zero)
    backgroundColor = theme.colors.elevated
    layer.cornerRadius = theme.radius.md
    image.cornerRadius = theme.radius.sm
    image.placeholderColor = attachment.kind == .image ? attachment.color ?? theme.colors.elevated2 : theme.colors.elevated2
    image.url = attachment.kind == .image ? attachment.uri : nil
    file.contentMode = .scaleAspectFit
    file.tintColor = attachment.color ?? theme.colors.secondaryLabel
    file.isHidden = attachment.kind == .image
    name.isAccessibilityElement = true
    name.accessibilityLabel = attachment.name
    detail.isAccessibilityElement = attachment.detail != nil
    detail.accessibilityLabel = attachment.detail
    remove.setImage(UIImage(systemName: "xmark", withConfiguration: UIImage.SymbolConfiguration(pointSize: 10, weight: .semibold)), for: .normal)
    remove.tintColor = theme.colors.label
    remove.backgroundColor = theme.colors.fill
    remove.layer.cornerRadius = 10
    remove.accessibilityLabel = labels.removeAttachment(attachment.name)
    remove.addTarget(self, action: #selector(removeAttachment), for: .touchUpInside)
    [image, file, name, detail, remove].forEach(addSubview)
  }

  @available(*, unavailable)
  public required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  struct Layout {
    let size: CGSize
    let imageFrame: CGRect
    let name: ShadowListKitTextLayout
    let nameFrame: CGRect
    let detail: ShadowListKitTextLayout?
    let detailFrame: CGRect
    let removeFrame: CGRect
  }

  static func layout(attachment: ShadowListUtilsAssistantAttachment, width: CGFloat, removable: Bool, theme: ShadowListUtilsTheme) -> Layout {
    let padding = theme.spacing.xs + 2
    let width = min(220, max(1, width))
    let textWidth = max(1, width - padding - 36 - theme.spacing.sm - theme.spacing.md - (removable ? 20 + theme.spacing.xs : 0))
    let name = theme.typography.footnote.with(.semibold).layout(attachment.name, width: textWidth, maxLines: 1)
    let detail = attachment.detail.flatMap { $0.isEmpty ? nil : theme.typography.caption.layout($0, width: textWidth) }
    let textHeight = name.size.height + (detail?.size.height ?? 0)
    let height = max(36, textHeight) + 2 * padding
    let actualWidth = min(width, padding + 36 + theme.spacing.sm + max(name.size.width, detail?.size.width ?? 0)
      + theme.spacing.md + (removable ? 20 + theme.spacing.xs : 0))
    let direction = ShadowListUtilsDirection(width: actualWidth)
    let textX = padding + 36 + theme.spacing.sm
    let textY = (height - textHeight) / 2
    return Layout(size: CGSize(width: actualWidth, height: height),
      imageFrame: direction.place(CGRect(x: padding, y: (height - 36) / 2, width: 36, height: 36)),
      name: name, nameFrame: direction.place(CGRect(CGPoint(x: textX, y: textY), name)), detail: detail,
      detailFrame: direction.place(detail.map { CGRect(CGPoint(x: textX, y: textY + name.size.height), $0) } ?? .zero),
      removeFrame: direction.place(CGRect(x: actualWidth - theme.spacing.md - 20, y: (height - 20) / 2, width: 20, height: 20)))
  }

  public override func sizeThatFits(_ size: CGSize) -> CGSize {
    Self.layout(attachment: attachment, width: size.width, removable: onRemove != nil, theme: theme).size
  }

  public override func layoutSubviews() {
    super.layoutSubviews()
    let layout = Self.layout(attachment: attachment, width: bounds.width, removable: onRemove != nil, theme: theme)
    image.frame = layout.imageFrame
    file.frame = layout.imageFrame.insetBy(dx: 9, dy: 9)
    name.show(layout.name, theme.colors.label)
    name.frame = layout.nameFrame
    detail.isHidden = layout.detail == nil
    if let text = layout.detail { detail.show(text, theme.colors.secondaryLabel) }
    detail.frame = layout.detailFrame
    remove.frame = layout.removeFrame
    remove.isHidden = onRemove == nil
  }

  @objc func removeAttachment() { onRemove?(attachment.key) }
}
