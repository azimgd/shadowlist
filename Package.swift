// swift-tools-version:5.9

import PackageDescription

/*
 * ShadowListKit, the UIKit list, and the C++ core it runs on. The core includes itself as
 * <shadowlist-core/...>, which resolves from the packages folder. Both targets add that folder
 * as a header search path. Only ShadowListKit is a product, and its public headers hold no C++.
 */
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
      // SwiftPM wants an existing public headers folder. The core's own holds nothing but the core.
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
