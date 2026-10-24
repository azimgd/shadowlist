/*
 * CoreText layout oracle. Reads JSON {"items":[{"id","text","size","kern","weight","width"}]}
 * from stdin and prints [{"id","lines","lastWidth"}]: the number of wrapped lines and the last
 * line's width when the text is laid out at that width in the system font, the same engine
 * React Native's iOS Text uses.
 */
import AppKit
import CoreText
import Foundation

struct Item: Decodable {
  let id: String
  let text: String
  let size: Double
  let kern: Double
  let weight: String
  let width: Double
}

struct Result: Encodable {
  let id: String
  let lines: Int
  let lastWidth: Double
}

let data = FileHandle.standardInput.readDataToEndOfFile()
let items = try JSONDecoder().decode([String: [Item]].self, from: data)["items"]!

var results: [Result] = []
for item in items {
  let weight: NSFont.Weight = item.weight == "semibold" ? .semibold : .regular
  let font = NSFont.systemFont(ofSize: item.size, weight: weight)
  let attributed = NSAttributedString(
    string: item.text,
    attributes: [.font: font, .kern: item.kern]
  )
  let framesetter = CTFramesetterCreateWithAttributedString(attributed)
  let path = CGPath(
    rect: CGRect(x: 0, y: 0, width: item.width, height: 100_000),
    transform: nil
  )
  let frame = CTFramesetterCreateFrame(framesetter, CFRange(location: 0, length: 0), path, nil)
  let lines = CTFrameGetLines(frame) as! [CTLine]
  let lastWidth = lines.last.map { CTLineGetTypographicBounds($0, nil, nil, nil) } ?? 0
  results.append(Result(id: item.id, lines: lines.count, lastWidth: lastWidth))
}

let out = try JSONEncoder().encode(results)
FileHandle.standardOutput.write(out)
