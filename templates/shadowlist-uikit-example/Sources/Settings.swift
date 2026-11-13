import Foundation

/*
 * Launch arguments, the same names the React Native example reads: -SLRoute Feed -SLCount 1000
 * -SLDebug 1. -SLEngine picks the list implementation.
 */
enum Engine: String, CaseIterable {
  /*
   * The ShadowListUtils template list of the screen, with sizes from its precomputed layouts.
   */
  case shadowlist = "sl"
  /*
   * ShadowListKitListView with the library's rows, measuring each through its cell's sizeThatFits
   * on the main thread.
   */
  case shadowlistAuto = "sl-auto"
  /*
   * UITableView hosting the library's row views, with heightForRowAt from the same precomputed
   * layouts. Grids use a UICollectionView with a custom column layout instead.
   */
  case table = "table"
  /*
   * UITableView with self-sizing rows and an estimated height. Grids use the custom column
   * layout with each row's height computed once on the main thread.
   */
  case tableAuto = "table-auto"

  var title: String {
    switch self {
    case .shadowlist: return "ShadowListKitListView"
    case .shadowlistAuto: return "ShadowListKitListView (self-sizing)"
    case .table: return "UITableView, UICollectionView in grids"
    case .tableAuto: return "UITableView (self-sizing), UICollectionView in grids"
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

  /*
   * -SLStates 1: the Feed and Chat screens also show what their fixtures lack: placeholder
   * posts, messages that are sending or failed, and captions.
   */
  static var states: Bool { defaults.string(forKey: "SLStates") == "1" }
}
