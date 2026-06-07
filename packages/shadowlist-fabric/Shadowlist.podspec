require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

# Mirror the canonical core into a local copy before pod installation. The dest is
# wiped first so it is always an exact mirror of the source: a per-file copy would
# leave behind files that were since removed from the canonical core, and those
# orphans would still be compiled in (s.source_files globs the whole dir).
core_source_dir = File.join(__dir__, "../shadowlist-core")
core_dest_dir = File.join(__dir__, "shadowlist-core")
if Dir.exist?(core_source_dir)
  FileUtils.rm_rf(core_dest_dir)
  FileUtils.mkdir_p(core_dest_dir)
  Dir.glob(File.join(core_source_dir, "**/*.{cpp,hpp}")).each do |source_file|
    relative_path = Pathname.new(source_file).relative_path_from(Pathname.new(core_source_dir))
    dest_file = File.join(core_dest_dir, relative_path)
    FileUtils.mkdir_p(File.dirname(dest_file))
    FileUtils.cp(source_file, dest_file)
  end
end

Pod::Spec.new do |s|
  s.name         = "Shadowlist"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported }
  s.source       = { :git => "https://github.com/azimgd/shadowlist.git", :tag => "#{s.version}" }

  s.source_files = [
    "ios/**/*.{h,m,mm,swift,cpp}",
    "cpp/**/*.{h,hpp,cpp}",
    "shadowlist-core/**/*.{hpp,cpp}",
  ]

  s.public_header_files = ["shadowlist-core/**/*.{h,hpp}"]
  s.private_header_files = ["ios/**/*.{h,hpp}", "cpp/**/*.{h,hpp}"]

  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '$(PODS_TARGET_SRCROOT)',
  }

  s.user_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => '$(PODS_ROOT)/Shadowlist'
  }

  install_modules_dependencies(s)
end
