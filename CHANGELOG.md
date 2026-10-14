# Changelog

## Unreleased

### ShadowList (React Native)

- `followAppends` keeps the end of an inverted list in view as rows grow in place, like a streaming reply, not only as rows are appended. A row inserted at the end is followed too.
- `onAtEndChange(atEnd)` reports a reader who scrolled an inverted list away from its end and back.
- `ItemActionsView` gives a view outside a list the swipe actions and menu of a list row.
- macOS: `contextMenuForItem` opens on a right click, and `enableSelectionOnKeyPress` with `onActivateItem` selects and activates rows from the keyboard.
- `shouldSelectItem` keeps rows like headers out of the selection.
- A horizontal list styled `height: 'auto'` takes the height of its tallest mounted row.

### ShadowListKit (iOS and Android)

- `followAppends` also keeps items growing at the end in view.

### ShadowListUtils

- `TemplateList` passes selection, keyboard selection, swipe actions, menus and `contentContainerStyle` through to the list.
- `Grouped.Actions`, and swipe actions and a menu on `Grouped.Row`.
- `Assistant.List` dismisses the keyboard on drag.

## 0.9.0

### ShadowListKit (iOS and Android)

- iOS: Swift package at the repo root (product `ShadowListKit`) and the `ShadowListKit` CocoaPods spec.
- Android: published as `io.github.azimgd:shadowlist-kit` on Maven Central and through JitPack.
- Android: the native library links the C++ runtime statically and is aligned for 16 KB page sizes.
