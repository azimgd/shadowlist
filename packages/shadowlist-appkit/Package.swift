// swift-tools-version:5.9

import PackageDescription

/*
 * The AppKit kit's tests, run with swift test on a Mac. The core and the kit come from the
 * package at the repo root.
 */
let package = Package(
  name: "ShadowListKitAppKitTests",
  platforms: [.macOS(.v14)],
  dependencies: [
    .package(name: "shadowlist", path: "../..")
  ],
  targets: [
    .testTarget(
      name: "ShadowListKitAppKitTests",
      dependencies: [
        .product(name: "ShadowListKitAppKit", package: "shadowlist")
      ],
      path: "Tests/ShadowListKitAppKitTests"
    )
  ]
)
