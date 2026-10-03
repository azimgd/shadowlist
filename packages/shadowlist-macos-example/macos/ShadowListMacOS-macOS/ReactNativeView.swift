import SwiftUI
import React_RCTAppDelegate

struct ReactNativeView: NSViewRepresentable {
  let factory: RCTReactNativeFactory

  func makeNSView(context: Context) -> NSView {
    factory.rootViewFactory.view(withModuleName: "ShadowListMacOS")
  }

  func updateNSView(_ nsView: NSView, context: Context) {}
}
