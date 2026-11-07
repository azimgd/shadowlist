# ShadowList for terminals

`ShadowList`, `SectionList` and `TreeList` in a terminal, with the same props and `ShadowListCommands` as React Native, and everything else of [shadowlist](../shadowlist-fabric/README.md)'s index.
React renders the rows, and the shared C++ core, reached through a Node-API addon, decides which rows mount and where
they sit. Rows are ordinary `react-native-tty` views. A 100,000 row list mounts what is near the window.

## Install

### Requirements

| Dependency | Version |
| --- | --- |
| `react` | `19.2.3` |
| Node | `>=22` |
| `react-native-tty` | From the ream repo, aliased as `react-native` by the consumer |
| CMake and a C++20 compiler | To build the addon |

```sh
yarn build:tty
```

`build/shadowlist_tty.node` is built against the headers of the Node that runs the command. It is ignored by git.

## Usage

```tsx
import { ShadowList, type ShadowListCommands } from 'shadowlist-tty';

const listRef = useRef<ShadowListCommands>(null);

<ShadowList
  ref={listRef}
  data={documents}
  keyExtractor={(document) => document.id}
  enableSelectionOnKeyPress
  onActivateItem={({ item }) => open(item)}
  renderItem={({ item, selected }) => (
    <Text style={{ fontWeight: selected ? 'bold' : 'normal' }}>{item.title}</Text>
  )}
/>;
```

Importing the package registers the view managers and the `ShadowListStore` module. See the
[React Native package](../shadowlist-fabric/README.md) for the props and commands.

### Bundling

The package ships TypeScript and is bundled by its consumer. With esbuild:

```js
await build({
  entryPoints: ['app.tsx'],
  bundle: true,
  platform: 'node',
  format: 'cjs',
  outfile: 'dist/app.cjs',
  jsx: 'automatic',
  define: { __DEV__: 'false', 'process.env.NODE_ENV': '"production"' },
  alias: {
    'react-native': 'react-native-tty',
    shadowlist: 'shadowlist-tty',
    react: '/one/copy/of/react',
  },
});
```

Set `SHADOWLIST_TTY_ADDON` to where `build/shadowlist_tty.node` is before the first list renders, as `SHADOWGRID_TTY_ADDON`
is for the grid. Unbundled, it is read from this package's `build/`. The addon is loaded with `process.dlopen`, so it does
not go through the bundler.

## In a terminal

| Input | What it does |
| --- | --- |
| Wheel | Scrolls by the cells the host reports. |
| Up, Down | With `enableSelectionOnKeyPress`, selects the row above or below and scrolls it into view. |
| PageUp, PageDown | The same, by the rows in the window. |
| Home, End | The same, to the first or last row. |
| Enter, Space | Calls `onActivateItem`. |
| Tab, a click | Gives a list with `enableSelectionOnKeyPress` the keyboard. Other lists never take focus. |

A row is one cell tall until it mounts, and then as tall as its layout. Offsets are whole cells, and `onScroll` comes at
most once per frame.

## What is not here

| Prop or command | In a terminal |
| --- | --- |
| `DraggableList`, `reorderEnabled` | `DraggableList` is a `ShadowList`, and `reorderEnabled` is inert. |
| Swipe actions, `contextMenuForItem`, `ItemActionsView` | Inert. `ItemActionsView` draws its children. |
| `KeyboardView`, `useKeyboardAnimation` | No soft keyboard. `KeyboardView` is a plain view and the animation stays at 0. |
| `onRefresh` | No pull to start one. `refreshing` still holds new data until it turns false. |
| `getItemSizeSpec`, `nonAnchorKeys` | Inert. Rows are measured when they mount. |
| `scrollEventThrottle`, `decelerationRate`, `bounces` | Inert. There is no momentum. |
| `showsVerticalScrollIndicator` | No scroll bar is drawn. |
| `animated` on a scroll command | Lands at once. |
| `prefetchRows` | Prefetch runs over the core's measured range, one window each way, not this count. |

## Tests

```sh
yarn test:tty
```

[test/run.mjs](test/run.mjs) bundles each test with esbuild as a consumer would, with `react-native` aliased to `../ream/packages/react-native-tty`, and runs it in that platform's test host. `react-native-tty`'s dependencies have to be installed in the `ream` checkout, which `yarn typecheck` reads as well. `ream.pin` names the commit CI checks it out at.

## How it fits together

The Fabric files in `shadowlist-fabric/src` load as they are, since `codegenNativeComponent` and
`codegenNativeCommands` exist on `react-native-tty`: there is no `*NativeComponent.tty.ts`. What native does on the other
platforms is here:

| File | What it holds |
| --- | --- |
| `cpp/ShadowListKitCoreNapi.cpp` | Hops into `ListDriver`, and the `ShadowListStore` functions over `RowKeyStore` and `ListSelection`. |
| `src/ShadowListKitCore.ts` | The addon's loader and one list's core. |
| `src/ShadowListKitListView.ts` | The three view managers. On each paint: runs the core, moves the children, sends the events. |
