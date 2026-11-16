import UIKit

final class BoardColumnHeading: UIView {
  private let titleLabel = UILabel()
  private let countLabel = UILabel()

  init(title: String, count: Int) {
    super.init(frame: .zero)
    titleLabel.attributedText = AppTheme.typography.headline.string(title)
    countLabel.attributedText = AppTheme.typography.subhead.with(.semibold).string(String(count), AppTheme.colors.secondaryLabel)
    countLabel.textAlignment = .right
    addSubview(titleLabel)
    addSubview(countLabel)
    isAccessibilityElement = true
    accessibilityTraits = .header
    accessibilityLabel = "\(title), \(count) cards"
  }
  required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: AppTheme.typography.headline.lineHeight + AppTheme.theme.spacing.md + AppTheme.theme.spacing.sm) }
  override func layoutSubviews() {
    super.layoutSubviews()
    titleLabel.frame = CGRect(x: AppTheme.theme.spacing.lg, y: AppTheme.theme.spacing.md, width: max(0, bounds.width - 60), height: AppTheme.typography.headline.lineHeight)
    countLabel.frame = CGRect(x: bounds.width - AppTheme.theme.spacing.lg - 32, y: AppTheme.theme.spacing.md + (AppTheme.typography.headline.lineHeight - AppTheme.typography.subhead.lineHeight) / 2, width: 32, height: AppTheme.typography.subhead.lineHeight)
  }
}
