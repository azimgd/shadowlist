import UIKit

/*
 * The example data, item for item the same as the React Native example's fixtures. Every
 * value is a pure function of the item's generation index, except the feed's time, which is
 * random there and (index % 24) hours here.
 */
enum Fixtures {
  static func imageURL(_ index: Int, width: Int) -> URL {
    let clean = FixtureStrings.images[index % FixtureStrings.images.count].replacingOccurrences(of: "https://", with: "")
    return URL(string: "https://images.weserv.nl/?url=\(clean)&q=60&w=\(width)")!
  }

  static func avatarColor(_ name: String) -> UIColor {
    var hash: Int64 = 0
    for unit in name.utf16 {
      hash = (hash * 31 + Int64(unit)) % 2_147_483_647
    }
    return Theme.avatarPalette[Int(hash % Int64(Theme.avatarPalette.count))]
  }

  static func initials(_ name: String) -> String {
    let words = name.split(separator: " ")
    guard let first = words.first?.first else { return "" }
    if words.count > 1, let last = words.last?.first {
      return "\(first)\(last)".uppercased()
    }
    return String(first).uppercased()
  }
}

struct Author {
  let name: String
  let initials: String
  let color: UIColor

  init(_ name: String) {
    self.name = name
    self.initials = Fixtures.initials(name)
    self.color = Fixtures.avatarColor(name)
  }
}

struct FeedPost {
  let id: String
  let author: Author
  let handle: String
  let text: String
  let time: String
  let images: [URL]

  init(index: Int) {
    let name = FixtureStrings.characterNames[index % FixtureStrings.characterNames.count]
    let imageCount = index % 10 == 0 ? 3 + index % 2 : 1
    id = "post-\(index)"
    author = Author(name)
    handle = "@" + name.lowercased().replacingOccurrences(of: " ", with: "")
    text = FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.count]
    let hours = index % 24
    time = hours == 0 ? "now" : "\(hours)h"
    images = (0..<imageCount).map { Fixtures.imageURL(index + $0, width: 800) }
  }
}

struct ChatMessage {
  let id: String
  let author: Author
  let isOwn: Bool
  let text: String?
  let images: [URL]
  var caption: String?

  init(id: String, author: Author, isOwn: Bool, text: String) {
    self.id = id
    self.author = author
    self.isOwn = isOwn
    self.text = text
    self.images = []
  }

  init(index: Int) {
    id = "msg-\(index)"
    isOwn = index % 3 != 0
    author = Author(isOwn ? "Me" : FixtureStrings.avatarNames[index % FixtureStrings.avatarNames.count])
    if index > 0 && index % 10 == 0 {
      images = (0..<4).map { Fixtures.imageURL(index + $0, width: 400) }
    } else if index > 0 && index % 5 == 0 {
      images = [Fixtures.imageURL(index, width: 800)]
    } else {
      images = []
    }
    text = images.isEmpty ? FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.count] : nil
  }
}

struct Contact {
  let id: String
  let author: Author
  let subtitle: String

  init(index: Int) {
    let names = FixtureStrings.characterNames.map { $0.split(separator: " ") }
    let first = names[index % names.count][0]
    let last = names[(index * 7 + index / 15) % names.count][1]
    id = "contact-\(index)"
    author = Author("\(first) \(last)")
    subtitle = "(\(100 + index % 900)) \(200 + index % 800)-\(1000 + index % 9000)"
  }
}

struct Photo {
  static let designHeights: [CGFloat] = [180, 220, 260, 200, 240, 280, 190, 230, 250, 210]

  let id: String
  let title: String
  let url: URL
  // Height over width of the image.
  let aspect: CGFloat

  init(index: Int) {
    id = "photo-\(index)"
    title = FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.count]
    url = Fixtures.imageURL(index, width: 400)
    let height = (400 * Photo.designHeights[index % Photo.designHeights.count] / 122).rounded()
    aspect = height / 400
  }
}

/*
 * "first item", "twenty second item": the chat's debug captions, numbered like itemOrdinals.ts.
 */
enum Ordinals {
  private static let ones = [
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
    "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
  ]
  private static let tens = ["", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"]
  private static let scales: [(Int, String)] = [(1_000_000_000, "billion"), (1_000_000, "million"), (1000, "thousand"), (100, "hundred")]
  private static let irregular = [
    "one": "first", "two": "second", "three": "third", "five": "fifth", "eight": "eighth", "nine": "ninth", "twelve": "twelfth",
  ]

  static func words(_ value: Int) -> [String] {
    if value < 20 { return [ones[value]] }
    if value < 100 {
      let rest = value % 10
      return [tens[value / 10]] + (rest > 0 ? [ones[rest]] : [])
    }
    for (scale, name) in scales where value >= scale {
      let rest = value % scale
      return words(value / scale) + [name] + (rest > 0 ? words(rest) : [])
    }
    return []
  }

  static func label(_ value: Int) -> String {
    var parts = words(value)
    let last = parts.removeLast()
    let ordinal: String
    if let word = irregular[last] {
      ordinal = word
    } else if last.hasSuffix("y") {
      ordinal = String(last.dropLast()) + "ieth"
    } else {
      ordinal = last + "th"
    }
    return (parts + [ordinal]).joined(separator: " ") + " item"
  }
}
