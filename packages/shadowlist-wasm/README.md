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

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `data` | `ReadonlyArray<T>` | required | `T` is currently constrained to `{ id: string }` |
| `renderElement` | `({ element, index }) => ReactNode` | required | Renders one item |
| `keyExtractor` | `(item, index) => string` | `item.id` | Use when your list key is not `id` |
| `style` | `CSSProperties` | `undefined` | Outer scroll container style |
| `elementStyle` | `CSSProperties` | `undefined` | Applied to each item wrapper |
| `ItemSeparatorComponent` | `ReactElement \| () => ReactElement \| null` | `undefined` | Rendered between items, not after the last (FlatList semantics) |

### Layout

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `horizontal` | `boolean` | `false` | Horizontal list |
| `inverted` | `boolean` | `false` | Bottom-up / chat-style list |
| `columns` | `number` | `1` | Multi-column layout |
| `stickyHeader` | `boolean` | `false` | Pins header to the viewport start |
| `stickyFooter` | `boolean` | `false` | Pins footer to the viewport end |
| `initialElementsSize` | `number` | `20` | Initial mounted window size |
| `containerOffsetIndex` | `number` | `-2` | Declarative scroll-to-index; `-2` (default) is inactive, set a non-negative index to scroll there |

### Callbacks

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `onScroll` | `({ nativeEvent }) => void` | `undefined` | Current content offset |
| `onStartReached` | `() => void` | `undefined` | Fires near the start edge |
| `onEndReached` | `() => void` | `undefined` | Fires near the end edge |
| `onStartReachedThreshold` | `number` | `1` | Fraction of viewport length |
| `onEndReachedThreshold` | `number` | `1` | Fraction of viewport length |
| `onViewableItemsChanged` | `({ viewableItems, changed }) => void` | `undefined` | FlatList-style viewability callback |
| `viewabilityConfig` | `{ itemVisiblePercentThreshold?: number }` | `undefined` | Percent of item that must be visible |

### Templates

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `ListHeaderComponent` | `ReactElement \| () => ReactElement \| null` | `undefined` | Header template |
| `ListFooterComponent` | `ReactElement \| () => ReactElement \| null` | `undefined` | Footer template |
| `ListEmptyComponent` | `ReactElement \| () => ReactElement \| null` | `undefined` | Empty-state template |

### Web-Specific

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `estimatedElementWidth` | `number` | `120` | Estimate used before an item is measured |
| `estimatedElementHeight` | `number` | `120` | Estimate used before an item is measured |

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
