// swift-tools-version:5.9

import PackageDescription

/*
 * The UIKit templates' tests, run with xcodebuild test on an iOS simulator. The core, the kit
 * and the templates come from the package at the repo root.
 */
let package = Package(
  name: "ShadowListUtilsTests",
  platforms: [.iOS(.v16)],
  dependencies: [
    .package(name: "shadowlist", path: "../..")
  ],
  targets: [
    .testTarget(
      name: "ShadowListUtilsTests",
      dependencies: [
        .product(name: "ShadowListUtils", package: "shadowlist")
      ],
      path: "Tests/ShadowListUtilsTests"
    )
  ]
)
