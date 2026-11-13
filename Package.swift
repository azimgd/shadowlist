// swift-tools-version:5.9

import PackageDescription

/*
 * The core includes itself as <shadowlist-core/...>, which resolves from the packages folder.
 * The AppKit kit links the platform neutral files of the UIKit kit, which compile once in each.
 * The ShadowListKit product builds for iOS only and ShadowListKitAppKit for macOS only. A Mac
 * app links ShadowListKitAppKit, and swift build on a Mac names it with --target.
 * ShadowListUtils is the templates library on ShadowListKit and builds for iOS only. Its C++
 * calls go through the Objective-C++ target ShadowListUtilsCore into ShadowListCoreUtils, the
 * core's template logic in utils. ShadowListCore leaves that folder out, and no kit links it.
 */
let package = Package(
  name: "ShadowListKit",
  platforms: [.iOS(.v16), .macOS(.v14)],
  products: [
    .library(name: "ShadowListKit", targets: ["ShadowListKit"]),
    .library(name: "ShadowListKitAppKit", targets: ["ShadowListKitAppKit"]),
    .library(name: "ShadowListUtils", targets: ["ShadowListUtils"]),
  ],
  targets: [
    .target(
      name: "ShadowListCore",
      path: "packages/shadowlist-core",
      exclude: ["sources.cmake", "utils"],
      // SwiftPM wants an existing public headers folder.
      publicHeadersPath: ".",
      cxxSettings: [
        .headerSearchPath("..")
      ]
    ),
    .target(
      name: "ShadowListKit",
      dependencies: ["ShadowListCore"],
      path: "packages/shadowlist-uikit/Sources/ShadowListKit",
      cxxSettings: [
        .headerSearchPath("../../..")
      ]
    ),
    .target(
      name: "ShadowListKitAppKit",
      dependencies: ["ShadowListCore"],
      path: "packages/shadowlist-appkit/Sources/ShadowListKit",
      cxxSettings: [
        .headerSearchPath("../../..")
      ]
    ),
    .target(
      name: "ShadowListCoreUtils",
      path: "packages/shadowlist-core/utils",
      publicHeadersPath: ".",
      cxxSettings: [
        .headerSearchPath("../..")
      ]
    ),
    .target(
      name: "ShadowListUtilsCore",
      dependencies: ["ShadowListCoreUtils"],
      path: "packages/shadowlist-uikit-utils/Sources/ShadowListUtilsCore",
      cxxSettings: [
        .headerSearchPath("../../..")
      ]
    ),
    .target(
      name: "ShadowListUtils",
      dependencies: ["ShadowListKit", "ShadowListUtilsCore"],
      path: "packages/shadowlist-uikit-utils/Sources/ShadowListUtils"
    ),
  ],
  cxxLanguageStandard: .cxx20
)
