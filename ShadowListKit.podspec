require "json"

# The kits share their version with the npm package.
package = JSON.parse(File.read(File.join(__dir__, "packages", "shadowlist-fabric", "package.json")))

Pod::Spec.new do |s|
  s.name         = "ShadowListKit"
  s.version      = package["version"]
  s.summary      = "A virtualized list for UIKit on the shadowlist C++ core."
  s.description  = "ShadowListKitListView is a UIScrollView with a UITableView-like data source. " \
                   "The visible content stays still while rows are measured, inserted above or removed."
  s.homepage     = "https://github.com/azimgd/shadowlist"
  s.license      = { :type => "MIT", :file => "LICENSE" }
  s.authors      = { "azimgd" => "me@azimgd.com" }
  s.platforms    = { :ios => "16.0" }
  s.source       = { :git => "https://github.com/azimgd/shadowlist.git", :tag => "v#{s.version}" }

  s.source_files = [
    "packages/shadowlist-uikit/Sources/ShadowListKit/**/*.{h,mm}",
    "packages/shadowlist-core/**/*.{hpp,cpp}",
  ]
  s.public_header_files = "packages/shadowlist-uikit/Sources/ShadowListKit/include/ShadowListKit/*.h"
  s.project_header_files = [
    "packages/shadowlist-uikit/Sources/ShadowListKit/Internal/*.h",
    "packages/shadowlist-core/**/*.hpp",
  ]

  s.frameworks   = "UIKit", "CoreText"
  s.libraries    = "c++"

  # The core includes itself as <shadowlist-core/...>, which resolves from the packages folder.
  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => '"$(PODS_TARGET_SRCROOT)/packages"',
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
  }
end
