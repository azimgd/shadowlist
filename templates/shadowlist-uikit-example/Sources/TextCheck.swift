import UIKit

/*
 * -SLTextCheck 1 logs every sample text whose computed height differs from what a UILabel
 * needs, the check that keeps precomputed row heights honest.
 */
enum TextCheck {
  static func run() {
    guard UserDefaults.standard.string(forKey: "SLTextCheck") == "1" else { return }
    let label = UILabel()
    label.numberOfLines = 0
    var mismatches = 0
    for style in [("subhead", TextStyle.subhead), ("body", TextStyle.body)] {
      for width in [306.0, 262.0, 233.5, 240.0] as [CGFloat] {
        for text in FixtureStrings.sampleTexts {
          label.attributedText = style.1.string(text)
          let fits = ceil(label.sizeThatFits(CGSize(width: width, height: .greatestFiniteMagnitude)).height)
          let computed = style.1.layout(text, width: width).size.height
          if fits != computed {
            mismatches += 1
            print("[TEXTCHECK] \(style.0) w=\(width) label=\(fits) computed=\(computed) text=\(text.prefix(30))")
          }
        }
      }
    }
    print("[TEXTCHECK] done mismatches=\(mismatches)")
  }
}
