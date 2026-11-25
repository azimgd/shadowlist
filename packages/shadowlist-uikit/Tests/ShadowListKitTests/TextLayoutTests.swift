import ShadowListKit
import UIKit
import XCTest

final class TextLayoutTests: XCTestCase {
  @MainActor
  func testRichTextPreservesRunColorsAndHitTestingUsesTheTypesetUtf16Indices() {
    let font = UIFont.systemFont(ofSize: 17)
    let rich = ShadowListKitTextLayout(
      attributedString: NSAttributedString(string: "Red", attributes: [.foregroundColor: UIColor.red]),
      font: font,
      lineHeight: 22,
      kern: 0,
      width: 200,
      maximumLines: 0,
      alignment: .natural
    )
    let plain = ShadowListKitTextLayout(
      string: "Red",
      font: font,
      lineHeight: 22,
      kern: 0,
      width: 200,
      maximumLines: 0,
      alignment: .natural
    )
    func pixels(_ layout: ShadowListKitTextLayout, _ color: UIColor) -> Data? {
      UIGraphicsImageRenderer(size: CGSize(width: 200, height: 30)).image { context in
        layout.draw(at: .zero, in: context.cgContext, color: color.cgColor)
      }.pngData()
    }
    XCTAssertEqual(pixels(rich, .blue), pixels(rich, .red))
    XCTAssertNotEqual(pixels(plain, .blue), pixels(plain, .red))
    let multiline = ShadowListKitTextLayout(
      string: "First\nSecond",
      font: font,
      lineHeight: 22,
      kern: 0,
      width: 200,
      maximumLines: 0,
      alignment: .natural
    )
    let second = multiline.rectForLine(at: 1)
    XCTAssertEqual(multiline.characterIndex(at: CGPoint(x: second.minX + 0.1, y: second.midY)), 6)
    XCTAssertEqual(multiline.characterIndex(at: CGPoint(x: 1000, y: 5)), UInt(NSNotFound))
    let truncated = ShadowListKitTextLayout(
      string: "A very long line that needs truncation",
      font: font,
      lineHeight: 22,
      kern: 0,
      width: 70,
      maximumLines: 1,
      alignment: .natural
    )
    let last = truncated.rectForLine(at: 0)
    XCTAssertEqual(truncated.characterIndex(at: CGPoint(x: last.maxX - 0.1, y: last.midY)), UInt(NSNotFound))
  }
}
