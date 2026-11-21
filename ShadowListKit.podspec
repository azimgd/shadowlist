require "json"

package = JSON.parse(File.read(File.join(__dir__, "packages", "shadowlist-fabric", "package.json")))

Pod::Spec.new do |s|
  s.name         = "ShadowListKit"
  s.version      = package["version"]
  s.summary      = "A virtualized list for UIKit and AppKit on the shadowlist C++ core."
  s.description  = "ShadowListKitListView is a UIScrollView with a UITableView-like data source. " \
                   "The visible content stays still while rows are measured, inserted above or removed."
  s.homepage     = "https://github.com/azimgd/shadowlist"
  s.license      = { :type => "MIT", :file => "LICENSE" }
  s.authors      = { "azimgd" => "me@azimgd.com" }
  s.platforms    = { :ios => "16.0", :osx => "14.0" }
  s.source       = { :git => "https://github.com/azimgd/shadowlist.git", :tag => "v#{s.version}" }

  # The AppKit kit links the platform neutral files of the UIKit kit, which compile once in each.
  s.source_files = "packages/shadowlist-core/**/*.{hpp,cpp}"
  s.project_header_files = "packages/shadowlist-core/**/*.hpp"

  s.ios.source_files = "packages/shadowlist-uikit/Sources/ShadowListKit/**/*.{h,mm}"
  s.ios.public_header_files = "packages/shadowlist-uikit/Sources/ShadowListKit/include/ShadowListKit/*.h"
  s.ios.project_header_files = "packages/shadowlist-uikit/Sources/ShadowListKit/Internal/*.h"
  s.ios.frameworks = "UIKit", "CoreText"

  s.osx.source_files = "packages/shadowlist-appkit/Sources/ShadowListKit/**/*.{h,mm}"
  s.osx.public_header_files = "packages/shadowlist-appkit/Sources/ShadowListKit/include/ShadowListKit/*.h"
  s.osx.project_header_files = "packages/shadowlist-appkit/Sources/ShadowListKit/Internal/*.h"
  s.osx.frameworks = "AppKit", "CoreText", "QuartzCore"

  s.libraries    = "c++"

  # The core includes itself as <shadowlist-core/...>, which resolves from the packages folder.
  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => '"$(PODS_TARGET_SRCROOT)/packages"',
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
  }
end
