require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

# Copy the core into this package once per pod install, which reads the podspec several times.
$shadowlist_core_vendored ||= system("node", File.join(__dir__, "scripts", "vendor-core.js"), exception: true)

Pod::Spec.new do |s|
  s.name         = "ShadowList"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  macos_version  = respond_to?(:min_macos_version_supported, true) ? min_macos_version_supported : "11.0"
  s.platforms    = { :ios => min_ios_version_supported, :osx => macos_version }
  s.source       = { :git => "https://github.com/azimgd/shadowlist.git", :tag => "#{s.version}" }

  s.source_files = [
    "ios/**/*.{h,m,mm,swift,cpp}",
    "cpp/**/*.{h,hpp,cpp}",
    "shadowlist-core/**/*.{hpp,cpp}",
  ]

  s.public_header_files = ["shadowlist-core/**/*.{h,hpp}"]
  s.private_header_files = ["ios/**/*.{h,hpp}", "cpp/**/*.{h,hpp}"]

  # The list's C++ runs on every commit, so Release builds optimize it for speed, not size.
  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '$(PODS_TARGET_SRCROOT)',
    'GCC_OPTIMIZATION_LEVEL[config=Release]' => '3',
  }

  s.user_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '$(PODS_ROOT)/ShadowList'
  }

  # In the monorepo, refresh the copy before every build so core edits reach iOS without
  # pod install. New or removed files still need pod install.
  s.script_phases = [{
    :name => "Sync shadowlist-core",
    :execution_position => :before_compile,
    :always_out_of_date => "1",
    :script => <<~'SCRIPT',
      CORE="${PODS_TARGET_SRCROOT}/../shadowlist-core"
      if [ -f "${CORE}/Container.cpp" ]; then
        rsync -a --delete --include='*/' --include='*.cpp' --include='*.hpp' --include='*.cmake' --exclude='*' \
          "${CORE}/" "${PODS_TARGET_SRCROOT}/shadowlist-core/"
      fi
    SCRIPT
  }]

  install_modules_dependencies(s)
end
