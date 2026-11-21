// swift-tools-version:5.9

import PackageDescription

/*
 * The core includes itself as <shadowlist-core/...>. The ShadowListCore target builds from
 * packages/shadowlist-core-include, which holds one link named shadowlist-core to the core
 * folder and is the public headers folder. A target in another package that depends on the
 * ShadowListCore product includes the core headers with the same spelling, for example
 * <shadowlist-core/host/RowSizeSpec.hpp>.
 * The AppKit kit links the platform neutral files of the UIKit kit, which compile once in each.
 * The ShadowListKit product builds for iOS only and ShadowListKitAppKit for macOS only. A Mac
 * app links ShadowListKitAppKit, and swift build on a Mac names it with --target.
 */
let package = Package(
  name: "ShadowListKit",
  platforms: [.iOS(.v16), .macOS(.v14)],
  products: [
    .library(name: "ShadowListKit", targets: ["ShadowListKit"]),
    .library(name: "ShadowListKitAppKit", targets: ["ShadowListKitAppKit"]),
    .library(name: "ShadowListCore", targets: ["ShadowListCore"]),
  ],
  targets: [
    .target(
      name: "ShadowListCore",
      path: "packages/shadowlist-core-include",
      exclude: ["shadowlist-core/sources.cmake"],
      publicHeadersPath: "."
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
  ],
  cxxLanguageStandard: .cxx20
)
