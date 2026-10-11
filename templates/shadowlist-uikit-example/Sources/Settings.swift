import Foundation

/*
 * Launch arguments, the same names the React Native example reads: -SLRoute Feed -SLCount 1000
 * -SLDebug 1. -SLEngine picks the list implementation.
 */
enum Engine: String, CaseIterable {
  /*
   * ShadowListKitListView with sizes from the precomputed layouts.
   */
  case shadowlist = "sl"
  /*
   * ShadowListKitListView measuring each row through its cell's sizeThatFits on the main thread.
   */
  case shadowlistAuto = "sl-auto"
  /*
   * UITableView with heightForRowAt from the same precomputed layouts.
   */
  case table = "table"
  /*
   * UITableView with self-sizing rows and an estimated height.
   */
  case tableAuto = "table-auto"

  var title: String {
    switch self {
    case .shadowlist: return "ShadowListKitListView"
    case .shadowlistAuto: return "ShadowListKitListView (self-sizing)"
    case .table: return "UITableView"
    case .tableAuto: return "UITableView (self-sizing)"
    }
  }

  var isShadowList: Bool { self == .shadowlist || self == .shadowlistAuto }
  var selfSizing: Bool { self == .shadowlistAuto || self == .tableAuto }
}

enum Settings {
  private static let defaults = UserDefaults.standard

  static var route: String? { defaults.string(forKey: "SLRoute") }
  static var engine: Engine { Engine(rawValue: defaults.string(forKey: "SLEngine") ?? "") ?? .shadowlist }
  static var count: Int {
    let value = defaults.integer(forKey: "SLCount")
    return value > 0 ? value : 1000
  }
  static var debug: Bool { defaults.string(forKey: "SLDebug") == "1" }
  static var images: Bool { defaults.string(forKey: "SLImages") != "0" }
}
