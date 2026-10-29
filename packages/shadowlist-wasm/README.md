# ShadowListKit for the web

A virtualized list for the browser, written in TypeScript on the shadowlist core in C++ compiled to WebAssembly.
`ShadowListKitListView` is a custom element, `<shadowlist-kit-list-view>`, with a data source. The visible content
stays still while items are measured, inserted above or deleted. Items have stable keys.

## Install

```sh
cd packages/shadowlist-wasm
./build.sh
```

- `build.sh` needs Emscripten. It uses `emcmake` from the `PATH`, or sources `emsdk_env.sh` from `$EMSDK`
  (default `~/emsdk`).
- `dist/` then holds the ES modules, their types, `shadowlistkit.mjs` and `shadowlistkit.wasm`. Serve
  `shadowlistkit.wasm` next to `shadowlistkit.mjs`, or pass `locateFile` to `loadShadowListKit`.

## Usage

```ts
import {
  ShadowListKitListCell,
  ShadowListKitListView,
  loadShadowListKit,
} from 'shadowlist-wasm';

class PostCell extends ShadowListKitListCell {
  constructor() {
    super('post');
  }

  configure(post: Post): void {
    this.view.textContent = post.text;
  }
}

await loadShadowListKit();

const list = new ShadowListKitListView();
list.style.height = '100vh';
list.registerCell('post', () => new PostCell());
list.dataSource = {
  numberOfItems: () => posts.length,
  keyForItem: (_, index) => posts[index].id,
  cellForItem: (listView, index) => {
    const cell = listView.dequeueReusableCell<PostCell>('post');
    cell.configure(posts[index]);
    return cell;
  },
};
document.body.append(list);
list.reloadData();
```

## API

The data source gives `numberOfItems`, `keyForItem` and `cellForItem`, and optionally `sizeForItem` for sizes
without cells, `reconfigureCell` for payload reloads and `contentVersionForItem` for `applyChanges`. Without
`sizeForItem` every item is measured once through its cell's view at the list's cross size. The delegate has
`willDisplayCell`, `didEndDisplayingCell`, `didScroll`, `didEndScrolling`, `didReachStart` and `didReachEnd`, all
optional. `prefetchDataSource` hears `prefetchItems` and `cancelPrefetchingForItems`.

Properties: `inverted`, `followAppends`, `horizontal`, `numberOfColumns`, `estimatedItemSize`, `overscan`,
`mountOverscan`, `startReachedThreshold`, `endReachedThreshold`, `snapToItem`, `snapAlignment`
(`ShadowListKitSnapAlignment.START`, `CENTER` or `END`, the strings `'start'`, `'center'` and `'end'`),
`stickyIndices`, `headerView`, `footerView`, `contentCrossSize`. Data: `reloadData`, `insertItems`, `deleteItems`,
`reloadItems(indices, payload)`, `moveItem(sourceIndex, destinationIndex)`, `performBatchUpdates(updates, completion)`
with `completion(finished)`, `applyChanges`. Queries and commands: `cellForItem`, `visibleCells`, `visibleRange`,
`rectForItem` (content coordinates, header included), `contentSize`, `anchorState`, `scrollToItem`, `scrollToStart`,
`scrollToEnd`, `restoreAnchorState`. Sizes are in CSS pixels.

- Batches are planned by the core's `planBatch`: deletes, reloads and move sources are indices in the data before,
  inserts and move destinations in the data after. A batch that does not add up reloads everything. `applyChanges`
  diffs every key with the core's `diffKeys`, reloads items whose `contentVersionForItem` changed since the last
  reload, and returns a `ShadowListKitListChanges`. The core's `BatchCollector` and `ContentVersions` hold the batch
  and the versions. A number version compares as itself and a string version by a 64 bit hash. A batch made before
  the core loaded reads everything again when it ends.
- A cell's `view` is placed at an exact frame. An item whose content changed size needs `reloadItems`, which
  measures it again.
- The list needs a size from its styles. Only the scroll axis scrolls, and the scrollbar gutter stays reserved.
  `contentCrossSize` makes the content that wide (or tall, for a horizontal list) and lets the browser scroll across
  too, as a grid's rows need. `crossOffset` reads that position, and `didScroll` fires for either axis.
- Masonry is round robin: item `i` goes in column `i % numberOfColumns`, and each column stacks its own items.
- A list that connects before `loadShadowListKit` was called starts the load itself. Until it resolves, properties,
  `dataSource`, `registerCell`, `reloadData`, `performBatchUpdates` and `restoreAnchorState` are safe and wait for it.
  `insertItems`, `deleteItems`, `applyChanges`, `rectForItem` and the scroll commands throw.

The view's own behavior:

- The browser scrolls the list. Touches and a mouse press on the scrollbar count as dragging, momentum and keyboard
  scrolling as settling. The list rests on `scrollend`, or after 150 ms without a scroll event.
- The browser's scroll anchoring is off on the list (`overflow-anchor: none`). The core keeps the content in place.
- Sticky rows stick with CSS `position: sticky`. Each mounted sticky row sits in a section element that runs to
  the next sticky row, which pushes it away. The browser pins it on the thread that scrolls.
- `snapToItem` moves to the nearest item edge once the list came to rest.
- Animated scroll commands step once per animation frame. A wheel or a touch stops them.
- The core is created when the list lays out and destroyed when it disconnects. A list never connected leaves its
  core to a `FinalizationRegistry`.
- Accessibility: the list has `role="list"`, each cell `role="listitem"` with `aria-posinset` and `aria-setsize`. A list given another role before it connects, such as `rowgroup` in a grid, leaves its cells' roles and positions to the cells. A cell with its own role keeps it.

Not in the web kit yet: sections, selection, drag reorder, swipe actions, context menus, pull to refresh, separators
and change animations.

## Example app

The example app lives in [`templates/shadowlist-web-example`](../../templates/shadowlist-web-example). From the
repo root:

```sh
cd templates/shadowlist-web-example
npm run build   # or: ../../packages/shadowlist-wasm/build.sh && ../../node_modules/.bin/tsc -b
npm start       # serves the repo root on http://localhost:8080
```

Then open `http://localhost:8080/templates/shadowlist-web-example/?SLRoute=Chat&SLCount=1000`. `SLRoute` takes
`Feed`, `Chat`, `Masonry`, `Horizontal` and `Sticky`. The page puts the list on `window.list` for the console.

## Tests

`yarn test:wasm` at the repo root (`yarn workspace shadowlist-wasm run test`) runs `test/ShadowListKitCore.test.ts`
against `dist/` in Node 22 or newer, without a browser. Run `yarn build:wasm` first.

## Export names

The wasm exports are `ShadowListKitCore_native*` and `ShadowListKitListState_*`, after the classes they serve. This is
deliberate: `ShadowListKitCore.ts` mirrors Android's `ShadowListKitCore`, whose JNI functions carry the same
`native*` names. A call reads the same on both hosts. shadowgrid-wasm names its exports `shadowgrid_*` in
snake_case instead, as a C API for any host. Neither is meant to be renamed to the other.

## Debug log

The core's `[SL]` log is off by default. `SHADOWLIST_DEBUG_LOG=1 ./build.sh` compiles it in and it prints to the
console on every pass.

## Source layout

| File | Role |
| --- | --- |
| `packages/shadowlist-core/host/ListDriver.{hpp,cpp}` | The core driven for a native list: layout passes, measurement, keys, sticky math, drag. |
| `cpp/ShadowListKitCoreWasm.cpp` | Thin wasm hops into the driver through a reused scratch area of the heap. |
| `src/ShadowListKitCore.ts` | Loads the module and wraps the wasm calls, the same shape as Android's `ShadowListKitCore`. |
| `src/ShadowListKitListView.ts` | Public API, layout pass, mounting, scroll commands. |
| `src/ShadowListKitListData.ts` | Keys, reloads, inserts, deletes, batches and `applyChanges`. |
| `src/ShadowListKitScrollGesture.ts` | The scroll phase from browser events, animated commands and the snap. |
| `src/ShadowListKitStickyPinning.ts` | Pins the active sticky row. |
| `src/ShadowListKitListCell.ts`, `ShadowListKitAnchorState.ts`, `ShadowListKitListChanges.ts` | Cell base class and public value types. |
| `src/ShadowListKitSnapAlignment.ts` | `snapAlignment` values, in the core's `SnapAlignment` order. |
