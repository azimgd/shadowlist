import AppKit

/*
 * One window with the screen -SLRoute names: Feed, a plain list, Driven, a list a host moves
 * the way a two axis grid does, or Sections, travellers under sticky section headers.
 */
final class AppDelegate: NSObject, NSApplicationDelegate {
  private var window: NSWindow!

  func applicationDidFinishLaunching(_ notification: Notification) {
    let screen: NSView & ListScreen
    switch Settings.route {
    case "Driven": screen = DrivenScreen(count: Settings.count)
    case "Sections": screen = SectionsScreen(count: Settings.count)
    default: screen = FeedScreen(count: Settings.count)
    }
    window = NSWindow(
      contentRect: NSRect(x: 200, y: 200, width: 420, height: 720), styleMask: [.titled, .closable, .resizable, .miniaturizable],
      backing: .buffered, defer: false)
    window.title = "SL AppKit: \(Settings.route)"
    switch Settings.theme {
    case "light": window.appearance = NSAppearance(named: .aqua)
    case "dark": window.appearance = NSAppearance(named: .darkAqua)
    default: break
    }
    window.contentView = screen
    window.makeKeyAndOrderFront(nil)
    NSApp.activate(ignoringOtherApps: true)
    Scenario.start(screen)
  }

  func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}
