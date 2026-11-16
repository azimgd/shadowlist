import ShadowListUtils

enum TripFileFixtures {
  private static let names = ["trips", "boarding-passes", "flights", "hotels", "photos", "visas", "receipts", "lisbon", "tokyo", "maps", "packing-lists", "notes"]
  private static let extensions = ["pdf", "ics", "jpg", "json", "txt", "gpx", "png", "heic"]
  static let roots = (0..<4).map { folder(name: names[$0], key: "root-\($0)", depth: 0) }

  private static func folder(name: String, key: String, depth: Int) -> ShadowListUtilsTreeNode {
    var children: [ShadowListUtilsTreeNode] = []
    if depth < 3 {
      children += (0..<2).map { folder(name: names[(depth * 3 + $0) % names.count], key: "\(key)/folder-\($0)", depth: depth + 1) }
    }
    children += (0..<4).map {
      ShadowListUtilsTreeNode(key: "\(key)/file-\($0)", name: "\(names[(depth + $0) % names.count])-\($0).\(extensions[(depth + $0) % extensions.count])")
    }
    return ShadowListUtilsTreeNode(key: key, name: name, children: children)
  }

  static func folderKeys(_ nodes: [ShadowListUtilsTreeNode]) -> Set<String> {
    Set(nodes.flatMap { node -> [String] in
      guard let children = node.children else { return [] }
      return [node.key] + Array(folderKeys(children))
    })
  }
}
