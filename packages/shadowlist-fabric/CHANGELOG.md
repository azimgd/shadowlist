# Changelog

## Unreleased

### Added

- `ShadowListNative`: a list whose rows are built in C++ from template elements and a JSI data
  store, so scrolling needs no React render per row. It takes `data`, `initialData` or `indexed`
  rows, `getExtra`, `stickyHeaderIndices`, `onElementPress`, `onElementLongPress` and the scroll
  commands, and exports `shadowListNativeProps` for binding any host component. See
  [SHADOWLIST_NATIVE.md](./SHADOWLIST_NATIVE.md).
- Core `scrollToStart`: goes to offset 0 with the header in view and holds the first row while
  it is measured.

### Changed

- When a data change removes the row being held in place (for example a refresh that drops the
  top post while adding new ones), the list holds the next row that was on screen. Before, it
  held nothing and the content jumped.
- A scroll command that is already at its target when it arrives (such as `scrollTo: 'start'`
  from offset 0) now stays there. Before, keeping the visible row in place could move the view
  to a row further down.
- Content that shrinks above the viewport no longer jumps to the end of the list when a visible
  row can be held in place.
- `scrollToIndex` keeps the rows on screen mounted until the jump lands, which removes a
  one-frame blank.
- iOS: scroll positions are reported while pull-to-refresh is showing, so rows added during a
  refresh no longer shift the list by the height of the spinner.
- iOS: a swipe that starts on a row cancels the press on that row, including when a scroll view
  around the list takes the swipe. A tap on a list still coasting after a fling stops the
  scroll instead of pressing a row.
- A React commit now reads the list's newest scroll state, which fixes content moving by a few
  points during a burst of prepends.
