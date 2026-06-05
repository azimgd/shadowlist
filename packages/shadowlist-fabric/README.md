# shadowlist

Virtualized list component for React Native Fabric.

`shadowlist` renders only the active window of items, measures the mounted rows, and adjusts scroll position when needed so dynamic-height feeds and chats stay stable while content settles.

## Install

```sh
yarn add shadowlist
```

## Basic Usage

```tsx
import { useRef } from 'react';
import { Text, View } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';

type Item = {
  id: string;
  title: string;
};

const data: Item[] = [
  { id: '1', title: 'First' },
  { id: '2', title: 'Second' },
  { id: '3', title: 'Third' },
];

export function Example() {
  const ref = useRef<ShadowlistCommands>(null);

  return (
    <Shadowlist
      ref={ref}
      data={data}
      renderElement={({ element }) => (
        <View style={{ padding: 16 }}>
          <Text>{element.title}</Text>
        </View>
      )}
    />
  );
}
```

## Props

### Data And Rendering

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `data` | `ReadonlyArray<T>` | required | `T` is currently constrained to `{ id: string }` |
| `renderElement` | `({ element, index }) => ReactElement` | required | Renders one item |
| `keyExtractor` | `(item, index) => string` | `item.id` | Use when your list key is not `id` |
| `style` | `ViewStyle` | `undefined` | Outer scroll view style |
| `elementStyle` | `ViewStyle` | `undefined` | Applied to each item wrapper |
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

`SectionList` groups items into sections with native, swapping sticky section
headers — the active header pins to the top of the viewport and is pushed out by
the next one as you scroll, pinned on the UI thread by the same C++ core that drives
the list. It flattens your `sections` into the one virtualized stream, so sections
cost nothing on top of `Shadowlist`.

```tsx
import { SectionList } from 'shadowlist';

<SectionList
  sections={[
    { key: 'A', title: 'A', data: [{ id: '1', name: 'Alice' }] },
    { key: 'B', title: 'B', data: [{ id: '2', name: 'Bob' }] },
  ]}
  renderItem={({ item }) => <Row item={item} />}
  renderSectionHeader={({ section }) => <SectionHeader title={section.title} />}
  ItemSeparatorComponent={<Separator />}
/>;
```

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `sections` | `Array<{ data: ItemT[]; key?: string } & SectionT>` | required | Each section's items plus your own fields |
| `renderItem` | `({ item, index, section }) => ReactElement` | — | `index` is the item's position within its section |
| `renderSectionHeader` | `({ section }) => ReactElement` | — | Rendered (and pinned) at each section start |
| `renderSectionFooter` | `({ section }) => ReactElement` | — | Rendered at each section end |
| `keyExtractor` | `(item, index) => string` | `item.id` | Per-section override via `section.keyExtractor` |
| `stickySectionHeadersEnabled` | `boolean` | `true` | Pin and swap section headers natively |
| `ItemSeparatorComponent` | `ReactElement \| () => ReactElement \| null` | — | Between items within a section |
| `SectionSeparatorComponent` | `ReactElement \| () => ReactElement \| null` | — | Between sections |

`ListHeaderComponent`, `ListFooterComponent`, `ListEmptyComponent`, `inverted`,
`onScroll`, `onStartReached` / `onEndReached`, the imperative ref (`scrollToIndex`,
`scrollToOffset`, `scrollToEnd`) all work as on `Shadowlist`.

## Examples

### Chat / Inverted

```tsx
<Shadowlist
  data={messages}
  inverted
  onStartReached={loadOlderMessages}
  renderElement={({ element }) => (
    <MessageBubble message={element} />
  )}
/>
```

### Sticky Header

```tsx
<Shadowlist
  data={rows}
  stickyHeader
  ListHeaderComponent={
    <View style={{ padding: 12, backgroundColor: 'white' }}>
      <Text>Contacts</Text>
    </View>
  }
  renderElement={({ element }) => <Row item={element} />}
/>
```

### Viewability

```tsx
<Shadowlist
  data={feed}
  viewabilityConfig={{ itemVisiblePercentThreshold: 60 }}
  onViewableItemsChanged={({ viewableItems, changed }) => {
    console.log('viewable', viewableItems.map((item) => item.key));
    console.log('changed', changed.map((item) => item.key));
  }}
  renderElement={({ element }) => <FeedCard item={element} />}
/>
```

### Imperative Scrolling

```tsx
const ref = useRef<ShadowlistCommands>(null);

ref.current?.scrollToIndex(200);
ref.current?.scrollToOffset(1200);
ref.current?.scrollToEnd(false);
```
