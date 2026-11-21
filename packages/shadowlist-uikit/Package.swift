// swift-tools-version:5.9

import PackageDescription

/*
 * The UIKit kit's tests, run with xcodebuild test on an iOS simulator. The core and the kit come
 * from the package at the repo root.
 */
let package = Package(
  name: "ShadowListKitTests",
  platforms: [.iOS(.v16)],
  dependencies: [
    .package(name: "shadowlist", path: "../..")
  ],
  targets: [
    .testTarget(
      name: "ShadowListKitTests",
      dependencies: [
        .product(name: "ShadowListKit", package: "shadowlist")
      ],
      path: "Tests/ShadowListKitTests"
    )
  ]
)
