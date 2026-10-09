import SwiftUI
import React_RCTAppDelegate

@main
struct ShadowListMacOSApp: App {
  @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

  var body: some Scene {
    Window("ShadowListMacOS", id: "main") {
      /*
       * Without an explicit floor, `.defaultSize` also becomes the window's minimum size at
       * this deployment target, and a programmatic shrink below it is refused.
       */
      ReactNativeView(factory: appDelegate.reactNativeFactory)
        .frame(minWidth: 480, minHeight: 320)
    }
    .defaultSize(width: 1280, height: 720)
    .windowResizability(.contentSize)
  }
}
