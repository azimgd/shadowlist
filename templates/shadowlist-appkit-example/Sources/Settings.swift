import Foundation

/*
 * Launch arguments, the same names the UIKit example reads: -SLRoute Feed -SLCount 1000
 * -SLTheme dark. -SLScenario runs a scripted check (Scenario.swift), and -SLBenchExit 1 quits
 * after it logs. -SLShot <path> writes a PNG of the window's content and quits.
 */
enum Settings {
  private static let defaults = UserDefaults.standard

  static var route: String { defaults.string(forKey: "SLRoute") ?? "Feed" }
  static var scenario: String? { defaults.string(forKey: "SLScenario") }
  static var count: Int {
    let value = defaults.integer(forKey: "SLCount")
    return value > 0 ? value : 1000
  }
  static var theme: String? { defaults.string(forKey: "SLTheme") }
  static var exitsAfterScenario: Bool { defaults.bool(forKey: "SLBenchExit") }
  static var shotPath: String? { defaults.string(forKey: "SLShot") }
}
