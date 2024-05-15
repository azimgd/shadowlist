# shadowlist (alpha release)

ShadowList is a new alternative to FlatList for React Native, created to address common performance issues and enhance the UX when dealing with large lists of data.
It invokes Yoga for precise layout measurements of Shadow Nodes and constructs a Fenwick Tree with layout metrics for efficient offset calculations. By virtualizing children and rendering only items within the visible area, ShadowList ensures optimal performance. It's built on Fabric and works with React Native version 0.74 and newer.

## Out of box comparison to FlatList
| Feature                       | ShadowList  | FlatList   |
|-------------------------------|-------------|------------|
| Smooth Scrolling              | ✅           | ❌         |
| Maintain Content Position     | ✅           | ❌         |
| No Content Flashing           | ✅           | ❌         |
| Long List initialScrollIndex  | ✅           | ❌         |
| Bi-directional Scrolling      | ✅           | ❌         |
| Native Inverted List Support  | ✅           | ❌         |
| 60 FPS Scrolling              | ✅           | ❌         |

## Scroll Performance
| Number of Items  | ShadowList                 | FlatList Speed       |
|------------------|----------------------------|----------------------|
| 100 (text only)  | 108mb memory - 60fps       | 164mb (38-43fps)     |
| 1000 (text only) | 186mb memory - 60fps       | 190mb (33-38fps)     |

## Installation
Add the package to your project via `yarn add shadowlist` and run `pod install` in the `ios` directory.

## Usage

```js
import ShadowListContainer from 'shadowlist';

<ShadowListContainer
  style={styles.container}
  ref={shadowListContainerRef}
  data={data}
  ListHeaderComponent={ListHeaderComponent}
  ListFooterComponent={ListFooterComponent}
  ListEmptyComponent={ListEmptyComponent}
  renderItem={({ item, index }) => (
    <CustomComponent item={item} index={index} />
  )}
/>
```

## API
| Prop                   | Type                     | Required | Description                                     |
|------------------------|--------------------------|----------|-------------------------------------------------|
| `data`                 | Array                    | Required | An array of data to be rendered in the list.    |
| `ListHeaderComponent`  | React component or null | Optional | A custom component to render at the top of the list. |
| `ListFooterComponent`  | React component or null | Optional | A custom component to render at the bottom of the list. |
| `ListEmptyComponent`  | React component or null | Optional | A custom component to render when the list is empty. |
| `renderItem`           | Function                 | Required | A function to render each item in the list. It receives an object with `item` and `index` properties. |
| `initialScrollIndex`   | Number                   | Optional | The initial index of the item to scroll to when the list mounts. |
| `inverted`             | Boolean                  | Optional | If true, the list will be rendered in an inverted order. |

## Contributing

See the [contributing guide](CONTRIBUTING.md) to learn how to contribute to the repository and the development workflow.

## License

MIT
