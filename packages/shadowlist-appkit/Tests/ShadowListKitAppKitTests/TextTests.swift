import AppKit
import ShadowListKitAppKit
import XCTest

/*
 * A text layout must be as tall as AppKit's own text measurement of the same string, the check
 * that keeps precomputed row heights honest. Each style is a font with a fixed line height and a
 * kern, as a row would use.
 */
final class TextTests: XCTestCase {
  private struct Style {
    let name: String
    let font: NSFont
    let lineHeight: CGFloat
    let kern: CGFloat

    func attributes() -> [NSAttributedString.Key: Any] {
      let paragraph = NSMutableParagraphStyle()
      paragraph.minimumLineHeight = lineHeight
      paragraph.maximumLineHeight = lineHeight
      return [.font: font, .kern: kern, .paragraphStyle: paragraph]
    }

    func layout(_ text: String, width: CGFloat, maximumLines: UInt = 0) -> ShadowListKitTextLayout {
      ShadowListKitTextLayout(
        string: text, font: font, lineHeight: lineHeight, kern: kern, width: width,
        maximumLines: maximumLines, alignment: .natural)
    }
  }

  private static let styles = [
    Style(name: "body", font: .systemFont(ofSize: 13), lineHeight: 16, kern: 0),
    Style(name: "headline", font: .systemFont(ofSize: 13, weight: .semibold), lineHeight: 16, kern: -0.08),
    Style(name: "subhead", font: .systemFont(ofSize: 11), lineHeight: 14, kern: 0.06),
    Style(name: "large", font: .systemFont(ofSize: 17), lineHeight: 22, kern: -0.43),
  ]

  private static let widths: [CGFloat] = [306, 262, 233.5, 240, 120]

  private static let texts = [
    "Window seat, clear skies, the mountains below. Perfect.",
    "Stuck on the runway for the third de-icing pass\u{2014}at least the sunrise is free.",
    "Boarding started twenty minutes late, and nobody at the gate seemed to mind. The windows were full of that pale blue you only get before dawn, a fuel truck idling below and a row of ground crew in orange vests laughing about something we couldn't hear.\n\nLow cloud at the other end, light rain, wind gusting from the southwest. The pilot promised a bumpy approach but a smooth cruise.",
    "Six countries in nine days and not a single checked bag. Everything fit in one carry-on and a small backpack: two pairs of shoes, a rain shell, a charger for everything, and far too many postcards to mail home.",
    "Missed my connection by six minutes. The desk put me on the next flight, found me a lounge pass and apologised twice.",
    "Every trip is a small reset. Phone off, window open, nothing to fix.",
    "Supercalifragilisticexpialidocious-and-then-some-unbreakable-words-to-wrap",
    "",
  ]

  /*
   * Height NSAttributedString measures for text wrapped at width.
   */
  private func measuredHeight(_ text: String, style: Style, width: CGFloat) -> CGFloat {
    let string = NSAttributedString(string: text, attributes: style.attributes())
    let bounds = string.boundingRect(
      with: CGSize(width: width, height: .greatestFiniteMagnitude),
      options: [.usesLineFragmentOrigin, .usesFontLeading])
    return ceil(bounds.height)
  }

  /*
   * Height a wrapping NSTextField needs for text in a frame width wide. The field's cell keeps a
   * line fragment padding on each side.
   */
  private func fieldHeight(_ text: String, style: Style, width: CGFloat) -> CGFloat {
    let field = NSTextField(wrappingLabelWithString: "")
    field.attributedStringValue = NSAttributedString(string: text, attributes: style.attributes())
    let size = field.cell!.cellSize(forBounds: NSRect(x: 0, y: 0, width: width, height: .greatestFiniteMagnitude))
    return ceil(size.height)
  }

  func testLayoutHeightMatchesAttributedStringMeasurement() {
    for style in Self.styles {
      for width in Self.widths {
        for text in Self.texts where !text.isEmpty {
          let computed = style.layout(text, width: width).size.height
          let measured = measuredHeight(text, style: style, width: width)
          XCTAssertEqual(computed, measured, "\(style.name) w=\(width) text=\(text.prefix(30))")
        }
      }
    }
  }

  func testLayoutHeightMatchesTextField() {
    let padding: CGFloat = 4
    for style in Self.styles {
      for width in Self.widths {
        for text in Self.texts where !text.isEmpty {
          let computed = style.layout(text, width: width).size.height
          let fits = fieldHeight(text, style: style, width: width + padding)
          XCTAssertEqual(computed, fits, "\(style.name) w=\(width) text=\(text.prefix(30))")
        }
      }
    }
  }

  func testLineCountIsHeightOverLineHeight() {
    for style in Self.styles {
      for text in Self.texts {
        let layout = style.layout(text, width: 240)
        XCTAssertEqual(layout.size.height, style.lineHeight * CGFloat(layout.lineCount))
        XCTAssertLessThanOrEqual(layout.size.width, 240)
      }
    }
  }

  func testMaximumLinesCapsTheLayout() {
    let style = Self.styles[0]
    let text = Self.texts[2]
    let full = style.layout(text, width: 200)
    let capped = style.layout(text, width: 200, maximumLines: 2)
    XCTAssertGreaterThan(full.lineCount, 2)
    XCTAssertEqual(capped.lineCount, 2)
    XCTAssertEqual(capped.size.height, style.lineHeight * 2)
  }

  func testEmptyTextHasNoLines() {
    let layout = Self.styles[0].layout("", width: 240)
    XCTAssertEqual(layout.lineCount, 0)
    XCTAssertEqual(layout.size, .zero)
  }

  @MainActor
  func testTextViewShowsItsLinesAsLayerContents() {
    let style = Self.styles[0]
    let view = ShadowListKitTextView(frame: NSRect(x: 0, y: 0, width: 240, height: 100))
    view.textLayout = style.layout(Self.texts[0], width: 240)
    view.layoutSubtreeIfNeeded()
    view.displayIfNeeded()
    XCTAssertNotNil(view.layer?.contents)
    view.textLayout = nil
    view.displayIfNeeded()
    XCTAssertNil(view.layer?.contents)
  }

  @MainActor
  func testTextViewDrawsAsynchronously() {
    let style = Self.styles[0]
    let view = ShadowListKitTextView(frame: NSRect(x: 0, y: 0, width: 240, height: 100))
    view.displaysAsynchronously = true
    view.textLayout = style.layout(Self.texts[0], width: 240)
    XCTAssertNil(view.layer?.contents)
    let drawn = expectation(description: "bitmap shown")
    func poll() {
      if view.layer?.contents != nil {
        drawn.fulfill()
      } else {
        DispatchQueue.main.async(execute: poll)
      }
    }
    poll()
    wait(for: [drawn], timeout: 5)
  }
}
