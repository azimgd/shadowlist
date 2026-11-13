import Foundation

// A separate module with SwiftPM, and part of this one with CocoaPods.
#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * Contacts grouped by the first character of their name. The title is that character after
 * trimming, upper cased, or # when the name is empty or the character is not a letter. A
 * character is a whole grapheme cluster, like a letter with its accents. Sections sort by title
 * in the reader's locale with # last, and items keep their order inside a section. The section
 * key is the title.
 */
public func shadowListUtilsGroupIntoSections(_ items: [ShadowListUtilsContactItem]) -> [ShadowListUtilsContactSection] {
  let otherTitle = ShadowListUtilsCore.sectionOtherTitle()
  var groups: [String: [ShadowListUtilsContactItem]] = [:]
  for item in items {
    groups[sectionTitle(item.name, otherTitle: otherTitle), default: []].append(item)
  }
  let titles = groups.keys.sorted { first, second in
    if first == otherTitle || second == otherTitle {
      return second == otherTitle && first != otherTitle
    }
    return first.localizedStandardCompare(second) == .orderedAscending
  }
  return titles.map { ShadowListUtilsContactSection(key: $0, title: $0, items: groups[$0] ?? []) }
}

/*
 * The core answers with the first code point: an ASCII letter in upper case, # for a name that
 * starts with no letter, and a code point outside ASCII as written. The title is the whole
 * grapheme cluster that code point starts, like a letter with its accents, in upper case when
 * it is a letter.
 */
private func sectionTitle(_ name: String, otherTitle: String) -> String {
  let initial = ShadowListUtilsCore.sectionInitial(forName: name)
  guard initial != otherTitle, let scalar = initial.unicodeScalars.first else { return otherTitle }
  let first = name.first { $0.unicodeScalars.first.map { String($0).uppercased() == String(scalar).uppercased() } ?? false }
  guard let first, first.isLetter else { return scalar.isASCII ? initial : otherTitle }
  return String(first).uppercased()
}
