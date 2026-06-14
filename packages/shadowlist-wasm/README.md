# shadowlist-wasm

Virtualized list component for React on the web, powered by the same shared core behavior as the React Native package.

`shadowlist-wasm` mounts the active window, measures the rendered DOM nodes, and keeps the viewport stable while item sizes settle. The public list API stays close to the Fabric package so app code can move between platforms with minimal change.

## Install

```sh
yarn add shadowlist-wasm
```

## Basic Usage

```tsx
import { useRef } from 'react';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist-wasm';

type Item = {
  id: string;
  title: string;
};

export function Example({ data }: { data: Item[] }) {
  const ref = useRef<ShadowlistCommands>(null);

  return (
    <Shadowlist
      ref={ref}
      data={data}
      style={{ height: 600 }}
      renderElement={({ element }) => (
        <div style={{ padding: 16 }}>{element.title}</div>
      )}
    />
  );
}
```

The component loads the WASM core asynchronously on first mount and renders an empty list until it resolves. Make sure your bundler serves the `.wasm` asset shipped with the package (most modern bundlers handle this automatically).

## Props

The list API mirrors the Fabric package, and the tables below follow the same grouping.

### Data And Rendering

| Prop                     | Type                                         | Default     | Notes                                                           |
| ------------------------ | -------------------------------------------- | ----------- | --------------------------------------------------------------- |
| `data`                   | `ReadonlyArray<T>`                           | required    | `T` is currently constrained to `{ id: string }`                |
| `renderElement`          | `({ element, index }) => ReactNode`          | required    | Renders one item                                                |
| `keyExtractor`           | `(item, index) => string`                    | `item.id`   | Use when your list key is not `id`                              |
| `style`                  | `CSSProperties`                              | `undefined` | Outer scroll container style                                    |
| `elementStyle`           | `CSSProperties`                              | `undefined` | Applied to each item wrapper                                    |
| `ItemSeparatorComponent` | `ReactElement \| () => ReactElement \| null` | `undefined` | Rendered between items, not after the last (FlatList semantics) |

### Layout

| Prop                   | Type      | Default | Notes                                                                                             |
| ---------------------- | --------- | ------- | ------------------------------------------------------------------------------------------------- |
| `horizontal`           | `boolean` | `false` | Horizontal list                                                                                   |
| `inverted`             | `boolean` | `false` | Bottom-up / chat-style list                                                                       |
| `columns`              | `number`  | `1`     | Multi-column layout                                                                               |
| `stickyHeader`         | `boolean` | `false` | Pins header to the viewport start                                                                 |
| `stickyFooter`         | `boolean` | `false` | Pins footer to the viewport end                                                                   |
| `initialElementsSize`  | `number`  | `20`    | Initial mounted window size                                                                       |
| `containerOffsetIndex` | `number`  | `-2`    | Declarative scroll-to-index; `-2` (default) is inactive, set a non-negative index to scroll there |

### Callbacks

| Prop                      | Type                                       | Default     | Notes                                |
| ------------------------- | ------------------------------------------ | ----------- | ------------------------------------ |
| `onScroll`                | `({ nativeEvent }) => void`                | `undefined` | Current content offset               |
| `onStartReached`          | `() => void`                               | `undefined` | Fires near the start edge            |
| `onEndReached`            | `() => void`                               | `undefined` | Fires near the end edge              |
| `onStartReachedThreshold` | `number`                                   | `1`         | Fraction of viewport length          |
| `onEndReachedThreshold`   | `number`                                   | `1`         | Fraction of viewport length          |
| `onViewableItemsChanged`  | `({ viewableItems, changed }) => void`     | `undefined` | FlatList-style viewability callback  |
| `viewabilityConfig`       | `{ itemVisiblePercentThreshold?: number }` | `undefined` | Percent of item that must be visible |

### Templates

| Prop                  | Type                                         | Default     | Notes                |
| --------------------- | -------------------------------------------- | ----------- | -------------------- |
| `ListHeaderComponent` | `ReactElement \| () => ReactElement \| null` | `undefined` | Header template      |
| `ListFooterComponent` | `ReactElement \| () => ReactElement \| null` | `undefined` | Footer template      |
| `ListEmptyComponent`  | `ReactElement \| () => ReactElement \| null` | `undefined` | Empty-state template |

### Web-Specific

| Prop                     | Type     | Default | Notes                                    |
| ------------------------ | -------- | ------- | ---------------------------------------- |
| `estimatedElementWidth`  | `number` | `120`   | Estimate used before an item is measured |
| `estimatedElementHeight` | `number` | `120`   | Estimate used before an item is measured |

## Commands

```ts
type ShadowlistCommands = {
  setStartReachedEnabled(enabled: boolean): void;
  setEndReachedEnabled(enabled: boolean): void;
  scrollToIndex(index: number): void;
  scrollToOffset(offset: number, animated?: boolean): void;
  scrollToEnd(animated?: boolean): void;
};
```

## SectionList

`SectionList` groups items into sections with swapping sticky section headers,
flattening your `sections` into the one virtualized stream — so sections cost nothing
on top of `Shadowlist`. The API mirrors the Fabric package.

```tsx
import { SectionList } from 'shadowlist-wasm';

<SectionList
  sections={[
    { key: 'A', title: 'A', data: [{ id: '1', name: 'Alice' }] },
    { key: 'B', title: 'B', data: [{ id: '2', name: 'Bob' }] },
  ]}
  style={{ height: 600 }}
  renderElement={({ element }) => <div className="row">{element.name}</div>}
  renderSectionHeader={({ section }) => (
    <div className="header">{section.title}</div>
  )}
/>;
```

| Prop                          | Type                                                | Default   | Notes                                                |
| ----------------------------- | --------------------------------------------------- | --------- | ---------------------------------------------------- |
| `sections`                    | `Array<{ data: ItemT[]; key?: string } & SectionT>` | required  | Each section's items plus your own fields            |
| `renderElement`               | `({ element, index, section }) => ReactNode`        | —         | `index` is the element's position within its section |
| `renderSectionHeader`         | `({ section }) => ReactNode`                        | —         | Rendered (and pinned) at each section start          |
| `renderSectionFooter`         | `({ section }) => ReactNode`                        | —         | Rendered at each section end                         |
| `keyExtractor`                | `(item, index) => string`                           | `item.id` | Per-section override via `section.keyExtractor`      |
| `stickySectionHeadersEnabled` | `boolean`                                           | `true`    | Pin and swap section headers                         |
| `ItemSeparatorComponent`      | `ReactElement \| () => ReactElement \| null`        | —         | Between items within a section                       |
| `SectionSeparatorComponent`   | `ReactElement \| () => ReactElement \| null`        | —         | Between sections                                     |

## TreeList

`TreeList` is a directory-browser / outline tree on the same engine. It flattens the
_visible_ subtree — every node whose ancestors are all expanded — into the one
virtualized stream, so only the rows on screen are mounted no matter how big the tree
is. Collapsed subtrees are never walked, and expand / collapse only changes the flat
key set, so measured row sizes are kept and the toggled row stays put as its children
appear.

```tsx
import { useState } from 'react';
import { TreeList } from 'shadowlist-wasm';

type Node = { id: string; name: string; children?: Node[] };

const tree: Node[] = [
  {
    id: '1',
    name: 'src',
    children: [
      { id: '1-1', name: 'index.ts' },
      {
        id: '1-2',
        name: 'components',
        children: [{ id: '1-2-1', name: 'Button.tsx' }],
      },
    ],
  },
];

export function FileTree() {
  const [expanded, setExpanded] = useState<Set<string>>(new Set(['1']));

  return (
    <TreeList
      data={tree}
      style={{ height: 600 }}
      getChildren={(node) => node.children}
      keyExtractor={(node) => node.id}
      expandedIds={expanded}
      onExpandedChange={setExpanded}
      renderElement={({ element, indent, hasChildren, isExpanded, toggle }) => (
        <div
          onClick={toggle}
          style={{ paddingLeft: indent, cursor: 'pointer' }}
        >
          {hasChildren ? (isExpanded ? '▾' : '▸') : '·'} {element.name}
        </div>
      )}
    />
  );
}
```

| Prop                 | Type                                           | Default     | Notes                                                                           |
| -------------------- | ---------------------------------------------- | ----------- | ------------------------------------------------------------------------------- |
| `data`               | `ReadonlyArray<T>`                             | required    | Root nodes                                                                      |
| `getChildren`        | `(item) => readonly T[] \| undefined`          | required    | A node's children, or `undefined` for a leaf                                    |
| `keyExtractor`       | `(item) => string`                             | required    | Globally unique, stable node id                                                 |
| `renderElement`      | `(info) => ReactNode`                          | required    | `info` carries `element, index, depth, indent, isExpanded, hasChildren, toggle` |
| `expandedIds`        | `ReadonlyArray<string> \| ReadonlySet<string>` | `undefined` | Controlled expansion set; pair with `onExpandedChange`                          |
| `initialExpandedIds` | `ReadonlyArray<string> \| ReadonlySet<string>` | `undefined` | Uncontrolled initial expansion                                                  |
| `onExpandedChange`   | `(expandedIds: Set<string>) => void`           | `undefined` | Fires with the next set on every toggle                                         |
| `indentWidth`        | `number`                                       | `16`        | Pixels of leading inset per depth level                                         |

`style`, `elementStyle`, `initialElementsSize`, `containerOffsetIndex`, `onScroll`,
`onStartReached` / `onEndReached`, `ItemSeparatorComponent` and the list templates all
work as on `Shadowlist`. The ref adds `scrollToNode(id)` on top of the usual commands.

## Drag To Reorder

Set `dragEnabled` to turn on drag-to-reorder (pointer-driven on the web, with edge
auto-scroll and a live shuffle). The list reports the final move through `onReorder`,
whose `data` is the already-reordered array — set it back into your state, or the row
snaps back on drop.

```tsx
const [data, setData] = useState(rows);

<Shadowlist
  data={data}
  style={{ height: 600 }}
  dragEnabled
  onReorder={({ data: reordered }) => setData(reordered)}
  renderElement={({ element }) => <Row item={element} />}
/>;
```

| Prop          | Type                           | Default     | Notes                                                                              |
| ------------- | ------------------------------ | ----------- | ---------------------------------------------------------------------------------- |
| `dragEnabled` | `boolean`                      | `false`     | Enable drag-to-reorder                                                             |
| `onReorder`   | `({ from, to, data }) => void` | `undefined` | Fires once on drop; `data` is the reordered array, `from` / `to` the moved indices |

## Examples

### Feed

```tsx
<Shadowlist
  data={feed}
  style={{ height: 720, overflow: 'auto' }}
  onEndReached={loadMore}
  renderElement={({ element }) => <FeedCard item={element} />}
/>
```

### Chat / Inverted

```tsx
<Shadowlist
  data={messages}
  style={{ height: 600 }}
  inverted
  onStartReached={loadOlderMessages}
  renderElement={({ element }) => <MessageBubble message={element} />}
/>
```

### Sticky Header And Footer

```tsx
<Shadowlist
  data={rows}
  style={{ height: 500 }}
  stickyHeader
  stickyFooter
  ListHeaderComponent={<div className="toolbar">Filters</div>}
  ListFooterComponent={<div className="composer">Reply</div>}
  renderElement={({ element }) => <Row item={element} />}
/>
```

### Viewability

```tsx
<Shadowlist
  data={feed}
  viewabilityConfig={{ itemVisiblePercentThreshold: 50 }}
  onViewableItemsChanged={({ viewableItems }) => {
    console.log(viewableItems.map((item) => item.key));
  }}
  renderElement={({ element }) => <FeedCard item={element} />}
/>
```

## Low-Level Core Access

The package also exports low-level helpers when you want direct access to the WASM handle:

```ts
import {
  createShadowlistCore,
  loadShadowlistCoreModule,
  type ShadowlistCoreInstance,
} from 'shadowlist-wasm';
```

Use this only if you are building a custom integration. Most apps should use the `<Shadowlist />` component.
