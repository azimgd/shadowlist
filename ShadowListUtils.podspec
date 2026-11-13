require "json"

package = JSON.parse(File.read(File.join(__dir__, "packages", "shadowlist-fabric", "package.json")))

Pod::Spec.new do |s|
  s.name         = "ShadowListUtils"
  s.version      = package["version"]
  s.summary      = "Themed list templates for ShadowListKit."
  s.description  = "ShadowListUtils is the templates package for ShadowListKit on UIKit: a theme, rows, primitives and " \
                   "the Masonry, Feed, Reorder, Snap, Nested, Contacts, Inbox and Chat lists."
  s.homepage     = "https://github.com/azimgd/shadowlist"
  s.license      = { :type => "MIT", :file => "LICENSE" }
  s.authors      = { "azimgd" => "me@azimgd.com" }
  s.platforms    = { :ios => "16.0" }
  s.source       = { :git => "https://github.com/azimgd/shadowlist.git", :tag => "v#{s.version}" }
  s.swift_version = "5.9"

  s.dependency "ShadowListKit", s.version.to_s

  # The C++ the Objective-C++ file calls is the core's template logic in utils, which compiles
  # here and not in the ShadowListKit pod. It reads headers of the core beside it.
  s.source_files = [
    "packages/shadowlist-uikit-utils/Sources/**/*.{swift,h,mm}",
    "packages/shadowlist-core/utils/*.{hpp,cpp}",
  ]
  s.public_header_files = "packages/shadowlist-uikit-utils/Sources/ShadowListUtilsCore/include/ShadowListUtilsCore/*.h"
  s.project_header_files = "packages/shadowlist-core/utils/*.hpp"
  s.preserve_paths = "packages/shadowlist-core/**/*.hpp"
  s.frameworks   = "UIKit"
  s.libraries    = "c++"

  # The core includes itself as <shadowlist-core/...>, which resolves from the packages folder.
  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => '"$(PODS_TARGET_SRCROOT)/packages"',
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
  }
end
