# Shadowlist

High-performance virtualized list component for React Native Fabric.

## API

| Prop | Type | Default | Description |
|------|------|---------|-------------|
| `data` | `ReadonlyArray<T>` | Required | Array of data items. Each item must have an `id` property. |
| `renderElement` | `(info: { element: T; index: number }) => ReactElement` | Required | Function to render each list item. |
| `style` | `ViewStyle` | `undefined` | Style for the list container. |
| `elementStyle` | `ViewStyle` | `undefined` | Style for each list element. |
| `inverted` | `boolean` | `false` | Renders list in reverse order (bottom to top). |
| `horizontal` | `boolean` | `false` | Renders list horizontally (left to right). |
| `columns` | `number` | `1` | Number of columns for multi-column layout. |
| `containerOffsetIndex` | `number` | `-2` | Offset index for container positioning. |
| `initialElementsSize` | `number` | `20` | Initial number of elements to render. |
| `onStartReached` | `() => void` | `undefined` | Called when scrolled to the start of the list. |
| `onEndReached` | `() => void` | `undefined` | Called when scrolled to the end of the list. |
| `ListHeaderComponent` | `ReactElement \| (() => ReactElement \| null) \| null` | `undefined` | Component to render at the top of the list. |
| `ListFooterComponent` | `ReactElement \| (() => ReactElement \| null) \| null` | `undefined` | Component to render at the bottom of the list. |
| `ListEmptyComponent` | `ReactElement \| (() => ReactElement \| null) \| null` | `undefined` | Component to render when the list is empty. |

## Commands

| Method | Parameters | Description |
|--------|------------|-------------|
| `scrollToIndex` | `index: number` | Scrolls to the specified index. |
| `setStartReachedEnabled` | `enabled: boolean` | Enables/disables start reached callback. |
| `setEndReachedEnabled` | `enabled: boolean` | Enables/disables end reached callback. |

## Usage

```tsx
import { Shadowlist } from 'shadowlist';

interface Item {
  id: string;
  title: string;
}

const data: Item[] = [
  { id: '1', title: 'Item 1' },
  { id: '2', title: 'Item 2' },
  { id: '3', title: 'Item 3' },
];

function App() {
  return (
    <Shadowlist
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

### Multi-column Layout

```tsx
<Shadowlist
  data={data}
  columns={3}
  renderElement={({ element }) => (
    <View style={{ padding: 8 }}>
      <Text>{element.title}</Text>
    </View>
  )}
/>
```

### Horizontal List

```tsx
<Shadowlist
  data={data}
  horizontal
  renderElement={({ element }) => (
    <View style={{ padding: 16 }}>
      <Text>{element.title}</Text>
    </View>
  )}
/>
```

### Inverted List (Chat)

```tsx
<Shadowlist
  data={messages}
  inverted
  renderElement={({ element }) => (
    <View style={{ padding: 12 }}>
      <Text>{element.text}</Text>
    </View>
  )}
/>
```

## Known Issues

### Animated Transform Not Supported

Using `transform` with Reanimated's `useAnimatedStyle` inside list elements can cause layout measurement issues and revision conflicts. 

**Workaround**: Use layout properties like `left`, `right`, `top`, or `marginLeft` instead of `transform` for animations within list elements.

```tsx
// Avoid this
const animatedStyle = useAnimatedStyle(() => ({
  transform: [{ translateX: offset.value }]
}));

// Use this instead
const animatedStyle = useAnimatedStyle(() => ({
  left: offset.value
}));
```
