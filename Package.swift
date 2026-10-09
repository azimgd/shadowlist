// swift-tools-version:5.9

import PackageDescription

// The core includes itself as <shadowlist-core/...>, which resolves from the packages folder.
let package = Package(
  name: "ShadowListKit",
  platforms: [.iOS(.v16)],
  products: [
    .library(name: "ShadowListKit", targets: ["ShadowListKit"])
  ],
  targets: [
    .target(
      name: "ShadowListCore",
      path: "packages/shadowlist-core",
      exclude: ["sources.cmake"],
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
  ],
  cxxLanguageStandard: .cxx20
)
