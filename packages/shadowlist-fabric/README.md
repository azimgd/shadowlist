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
| `autoHideHeader` | `boolean` | `false` | Header slides away as you scroll toward the content and back the other way (direction-based, native) |
| `autoHideFooter` | `boolean` | `false` | Footer slides away / back the same way |
| `initialElementsSize` | `number` | `20` | Initial mounted window size |
| `containerOffsetIndex` | `number` | `-2` | Initial scroll position / declarative scroll-to-index. `-2` (default) = inactive; a non-negative index opens at that row. See [Initial scroll position](#initial-scroll-position) |

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
`containerOffsetIndex`, `keyboardAvoidingEnabled` / `keyboardAvoidingOffset`,
`onScroll`, `onStartReached` / `onEndReached`, the imperative ref (`scrollToIndex`,
`scrollToOffset`, `scrollToEnd`) all work as on `Shadowlist`.

## TreeList

`TreeList` is a directory-browser / outline tree on the same engine. It flattens the
*visible* subtree — every node whose ancestors are all expanded — into the one
virtualized stream, so only the rows on screen are mounted no matter how big the tree
is. Collapsed subtrees are never walked, and expand / collapse only changes the flat
key set, so measured row sizes are kept and the toggled row stays put (maintain
visible content position) as its children appear.

```tsx
import { useState } from 'react';
import { Pressable, Text } from 'react-native';
import { TreeList } from 'shadowlist';

type Node = { id: string; name: string; children?: Node[] };

const tree: Node[] = [
  {
    id: '1',
    name: 'src',
    children: [
      { id: '1-1', name: 'index.ts' },
      { id: '1-2', name: 'components', children: [{ id: '1-2-1', name: 'Button.tsx' }] },
    ],
  },
];

export function FileTree() {
  const [expanded, setExpanded] = useState<Set<string>>(new Set(['1']));

  return (
    <TreeList
      data={tree}
      getChildren={(node) => node.children}
      keyExtractor={(node) => node.id}
      expandedIds={expanded}
      onExpandedChange={setExpanded}
      renderNode={({ item, indent, hasChildren, isExpanded, toggle }) => (
        <Pressable onPress={toggle} style={{ paddingLeft: indent }}>
          <Text>
            {hasChildren ? (isExpanded ? '▾' : '▸') : '·'} {item.name}
          </Text>
        </Pressable>
      )}
    />
  );
}
```

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `data` | `ReadonlyArray<T>` | required | Root nodes |
| `getChildren` | `(item) => readonly T[] \| undefined` | required | A node's children, or `undefined` for a leaf |
| `keyExtractor` | `(item) => string` | required | Globally unique, stable node id |
| `renderNode` | `(info) => ReactElement` | required | `info` carries `item, index, depth, indent, isExpanded, hasChildren, toggle` |
| `expandedIds` | `ReadonlyArray<string> \| ReadonlySet<string>` | `undefined` | Controlled expansion set; pair with `onExpandedChange` |
| `initialExpandedIds` | `ReadonlyArray<string> \| ReadonlySet<string>` | `undefined` | Uncontrolled initial expansion |
| `onExpandedChange` | `(expandedIds: Set<string>) => void` | `undefined` | Fires with the next set on every toggle |
| `indentWidth` | `number` | `16` | Pixels of leading inset per depth level |

`style`, `elementStyle`, `initialElementsSize`, `containerOffsetIndex`,
`keyboardAvoidingEnabled` / `keyboardAvoidingOffset`, `onScroll`,
`onStartReached` / `onEndReached`, `ItemSeparatorComponent` and the list templates all
work as on `Shadowlist`. The ref adds `scrollToNode(id)` on top of the usual commands.

## Drag To Reorder

Set `dragEnabled` to turn on native long-press drag-to-reorder. The pickup, finger
tracking, edge auto-scroll and live shuffle all run on the UI thread; the list reports
the final move through `onReorder`, whose `data` is the already-reordered array — set
it back into your state, or the row snaps back on drop.

```tsx
const [data, setData] = useState(rows);

<Shadowlist
  data={data}
  dragEnabled
  onReorder={({ data: reordered }) => setData(reordered)}
  renderElement={({ element }) => <Row item={element} />}
/>;
```

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `dragEnabled` | `boolean` | `false` | Enable long-press drag-to-reorder |
| `onReorder` | `({ from, to, data }) => void` | `undefined` | Fires once on drop; `data` is the reordered array, `from` / `to` the moved indices |

## Pull To Refresh

Provide `onRefresh` to enable pull-to-refresh and drive the indicator with the
controlled `refreshing` flag. It uses the platform-native control — `UIRefreshControl`
on iOS, `SwipeRefreshLayout` on Android — so the gesture and physics match the OS
exactly (this is what React Native's own `RefreshControl` does). Tint the indicator with
`refreshColor`. Vertical lists only; available on `Shadowlist`, `SectionList` and
`TreeList`.

```tsx
const [refreshing, setRefreshing] = useState(false);

const onRefresh = () => {
  setRefreshing(true);
  fetchLatest().then(() => setRefreshing(false));
};

<Shadowlist
  data={data}
  refreshing={refreshing}
  onRefresh={onRefresh}
  refreshColor="#FF9500"
  renderElement={({ element }) => <Row item={element} />}
/>;
```

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `onRefresh` | `() => void` | `undefined` | Fires on pull past the threshold; its presence enables the gesture |
| `refreshing` | `boolean` | `false` | Controlled state — set true on refresh, false when done |
| `refreshColor` | `ColorValue` | platform default | Tints the native indicator (the iOS spinner, the Android arc) |

For a **loading-more** spinner at the bottom (the `onEndReached` companion), there's no
dedicated prop — render an `ActivityIndicator` in `ListFooterComponent` while your load
is in flight:

```tsx
<Shadowlist
  data={data}
  onEndReached={loadMore}
  ListFooterComponent={loadingMore ? <ActivityIndicator /> : null}
  renderElement={({ element }) => <Row item={element} />}
/>
```

## Keyboard

Dependency-free — no `react-native-keyboard-controller`, no `reanimated`. Three tools:

| Tool | Use it for |
| --- | --- |
| `keyboardAvoidingEnabled` prop | A text input **inside** the list — the list slides content up to keep focused rows visible |
| `useKeyboardAnimation()` | Moving your **own** views (e.g. an external chat composer) with the keyboard |
| `KeyboardDismissView` | Dismissing the keyboard when the content is tapped |

### keyboardAvoidingEnabled (built-in list avoidance)

Available on `Shadowlist`, `SectionList` and `TreeList`. Vertical lists only.

| Prop | Type | Default | Notes |
| --- | --- | --- | --- |
| `keyboardAvoidingEnabled` | `boolean` | `false` | Grow the list's bottom inset by the keyboard overlap and slide content up; reverses on dismiss |
| `keyboardAvoidingOffset` | `number` | `0` | Pixels subtracted from the overlap (e.g. a tab bar or safe-area already below the list) |

```tsx
<Shadowlist
  data={data}
  keyboardAvoidingEnabled
  keyboardAvoidingOffset={insets.bottom}
  renderElement={({ element }) => <Row item={element} />}
/>
```

### useKeyboardAnimation (frame-accurate)

Returns `Animated.Value`s tracking the live keyboard frame — move your own views (a
composer, a backdrop) with it.

```ts
import { useKeyboardAnimation } from 'shadowlist';

const { height, progress } = useKeyboardAnimation();
// height: keyboard overlap in dp; progress: 0..1 transition
```

```tsx
// Lift an external composer + the (inverted) chat list as one unit:
const { height } = useKeyboardAnimation();
const translateY = height.interpolate({
  inputRange: [0, 1],
  outputRange: [0, -1], // move up by the keyboard height
});

<Animated.View style={{ flex: 1, transform: [{ translateY }] }}>
  <Shadowlist data={messages} inverted renderElement={...} />
  <Composer />
</Animated.View>;
```

- Don't use the native driver on styles that consume these values (they update from JS).
- **Android**: tracks every frame, including interactive dismiss; the host Activity needs `adjustResize`.
- **iOS**: matches the system open/close animation (interactive drag not yet tracked).
- Values stay at `0` if the native module isn't built (no throw).

### KeyboardDismissView (tap to dismiss)

Dismisses the keyboard when its content is tapped. Only intercepts while the keyboard
is open, so scrolling is otherwise untouched. Inputs and buttons keep working; keep
your composer outside it so its bar taps don't dismiss.

```tsx
import { KeyboardDismissView } from 'shadowlist';

<KeyboardDismissView style={{ flex: 1 }}>
  <Shadowlist data={messages} inverted renderElement={...} />
</KeyboardDismissView>;
```

Props: any `ViewProps`, plus `enabled?: boolean` (default `true`) to turn interception
off.

### useKeyboardInset (low-level)

The hook behind `keyboardAvoidingEnabled` — returns the keyboard's overlap (px) with a
measured view, to wire up yourself.

```ts
import { useKeyboardInset } from 'shadowlist';

const inset = useKeyboardInset(viewRef, { enabled: true, offset: 0 });
```

## Examples

### Initial Scroll Position

`containerOffsetIndex` opens the list already scrolled to a row — no scroll-from-top, no
blank flash. Keep it constant for a pure initial position; `scrollToIndex` overrides it
afterward.

```tsx
<Shadowlist
  data={data}
  containerOffsetIndex={30} // open at item 30
  renderElement={({ element }) => <Row item={element} />}
/>
```

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
