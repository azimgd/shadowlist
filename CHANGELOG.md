# Changelog

## Unreleased

### ShadowListKit for the web

- New `shadowlist-wasm` package: `ShadowListKitListView`, the custom element `<shadowlist-kit-list-view>`, on the
  core compiled to WebAssembly with Emscripten. Keys, measurement through cells or `sizeForItem`, reloads, inserts,
  deletes, batches, `applyChanges`, inverted lists, grids, horizontal lists, sticky rows, header and footer views,
  prefetching, snapping, saved positions and scroll commands.
- `templates/shadowlist-web-example` shows Feed, Chat, Masonry, Horizontal and Sticky screens.

## 0.9.0

### ShadowListKit (iOS and Android)

- iOS: Swift package at the repo root (product `ShadowListKit`) and the `ShadowListKit` CocoaPods spec.
- Android: published as `io.github.azimgd:shadowlist-kit` on Maven Central and through JitPack.
- Android: the native library links the C++ runtime statically and is aligned for 16 KB page sizes.
