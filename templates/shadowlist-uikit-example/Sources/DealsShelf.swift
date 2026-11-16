import UIKit

/*
 * The leading Explore carousel uses the same variable card widths and fares as RN.
 */
final class DealsShelf: UIView {
  private let scroll = UIScrollView()
  private let heading = UILabel()
  private let subtitle = UILabel()
  private var cards: [UILabel] = []
  private let cities = ["Lisbon", "Reykjavik", "Kyoto", "Marrakesh", "Tromsø", "Oaxaca", "Tbilisi", "Hobart", "Porto", "Seoul", "Cusco", "Valletta"]

  override init(frame: CGRect) {
    super.init(frame: frame)
    scroll.showsHorizontalScrollIndicator = false
    addSubview(scroll)
    heading.attributedText = AppTheme.typography.title3.string("Deals")
    subtitle.attributedText = AppTheme.typography.subhead.string("Fares this week", AppTheme.colors.secondaryLabel)
    scroll.addSubview(heading)
    scroll.addSubview(subtitle)
    let colors: [UIColor] = [UIColor(red: 0.184, green: 0.435, blue: 0.922, alpha: 1), UIColor(red: 0.510, green: 0.314, blue: 0.875, alpha: 1), .systemPink, .systemOrange, .systemGreen, .systemRed]
    for index in 0..<30 {
      let card = UILabel()
      card.numberOfLines = 2
      card.attributedText = AppTheme.typography.subhead.string("\(cities[index % cities.count])\nfrom $\(89 + (index * 53) % 400)", .white)
      card.backgroundColor = colors[index % colors.count]
      card.layer.cornerRadius = AppTheme.theme.radius.lg
      card.clipsToBounds = true
      let container = UIView()
      container.backgroundColor = card.backgroundColor
      container.layer.cornerRadius = AppTheme.theme.radius.lg
      container.addSubview(card)
      scroll.addSubview(container)
      cards.append(card)
    }
  }
  required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
  override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: 196) }
  override func sizeThatFits(_ size: CGSize) -> CGSize { CGSize(width: size.width, height: 196) }
  override func layoutSubviews() {
    super.layoutSubviews()
    scroll.frame = CGRect(x: 0, y: 0, width: bounds.width, height: 180)
    // The two lines are centered in the shelf. React Native floors a text origin to the pixel grid.
    let headingHeight = AppTheme.typography.title3.lineHeight
    let subtitleHeight = AppTheme.typography.subhead.lineHeight
    let scale = UIScreen.main.scale
    let top = floor((scroll.frame.height - headingHeight - subtitleHeight) / 2 * scale) / scale
    heading.frame = CGRect(x: 16, y: top, width: 134, height: headingHeight)
    subtitle.frame = CGRect(x: 16, y: top + headingHeight, width: 134, height: subtitleHeight)
    var x: CGFloat = 156
    for (index, card) in cards.enumerated() {
      let width = CGFloat(110 + (index * 37) % 150)
      card.superview?.frame = CGRect(x: x, y: 8, width: width, height: 164)
      card.frame = CGRect(x: 12, y: 112, width: width - 24, height: 40)
      x += width + 12
    }
    scroll.contentSize = CGSize(width: x - 6, height: 180)
  }
}
