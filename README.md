# shadowlist (ios alpha release)

ShadowList is a new alternative to FlatList for React Native, created to address common performance issues and enhance the UX when dealing with large lists of data.
It invokes Yoga for precise layout measurements of Shadow Nodes and constructs a Fenwick Tree with layout metrics for efficient offset calculations. By virtualizing children and rendering only items within the visible area, ShadowList ensures optimal performance. It's built on Fabric and works with React Native version 0.74 and newer.

## Out of box comparison to FlatList
| Feature                          | ShadowList   | FlatList   |
|----------------------------------|--------------|------------|
| 60 FPS Scrolling                 | ✅           | ❌         |
| No Content Flashing              | ✅           | ❌         |
| No Sidebar Indicator Jump        | ✅           | ❌         |
| Fast initialScrollIndex          | ✅           | ❌         |
| Nested ShadowList (ScrollView)   | ✅           | ❌         |
| Natively Inverted List Support   | ✅           | ❌         |
| Smooth Scrolling                 | ✅           | ❌         |

## Scroll Performance
| Number of Items  | ShadowList                 | FlatList             | FlashList            |
|------------------|----------------------------|----------------------|----------------------|
| 100 (text only)  | **156mb memory - 60fps**   | 195mb (38-43fps)     | ~~180mb (56fps)~~*   |
| 1000 (text only) | **187mb memory - 60fps**   | 200mb (33-38fps)     | ~~180mb (56fps)~~*   |

> **FlashList is unreliable and completely breaks when scrolling, resulting in unrealistic metrics.*  
> Given measurements show memory usage and FPS on fully loaded content, see demo [here](https://github.com/azimgd/shadowlist/issues/1) and implementation details [here](https://github.com/azimgd/shadowlist/blob/main/example/src/App.tsx).

## Installation
- CLI: Add the package to your project via `yarn add shadowlist` and run `pod install` in the `ios` directory.
- Expo: Add the package to your project via `npx expo install shadowlist` and run `npx expo prebuild` in the root directory.


## Usage

```js
import ShadowListContainer from 'shadowlist';

<ShadowListContainer
  contentContainerStyle={styles.container}
  ref={shadowListContainerRef}
  data={data}
  ListHeaderComponent={ListHeaderComponent}
  ListHeaderComponentStyle={styles.ListHeaderComponentStyle}
  ListFooterComponent={ListFooterComponent}
  ListHeaderComponentStyle={styles.ListFooterComponentStyle}
  ListEmptyComponent={ListEmptyComponent}
  ListEmptyComponentStyle={styles.ListFooterComponentStyle}
  renderItem={({ item, index }) => (
    <CustomComponent item={item} index={index} />
  )}
/>
```

## API
| Prop                       | Type                      | Required | Description                                     |
|----------------------------|---------------------------|----------|-------------------------------------------------|
| `data`                     | Array                     | Required | An array of data to be rendered in the list.    |
| `contentContainerStyle`    | ViewStyle                 | Optional | These styles will be applied to the scroll view content container which wraps all of the child views.  |
| `ListHeaderComponent`      | React component or null   | Optional | A custom component to render at the top of the list. |
| `ListHeaderComponentStyle` | ViewStyle                 | Optional | Styling for internal View for `ListHeaderComponent` |
| `ListFooterComponent`      | React component or null   | Optional | A custom component to render at the bottom of the list. |
| `ListFooterComponentStyle` | ViewStyle                 | Optional | Styling for internal View for `ListFooterComponent` |
| `ListEmptyComponent`       | React component or null   | Optional | A custom component to render when the list is empty. |
| `ListEmptyComponentStyle`  | ViewStyle                 | Optional | Styling for internal View for `ListEmptyComponent` |
| `renderItem`               | Function                  | Required | A function to render each item in the list. It receives an object with `item` and `index` properties. |
| `initialScrollIndex`       | Number                    | Optional | The initial index of the item to scroll to when the list mounts. |
| `inverted`                 | Boolean                   | Optional | If true, the list will be rendered in an inverted order. |
| `horizontal`               | Boolean                   | Optional | If true, renders items next to each other horizontally instead of stacked vertically. |
| `onBatchLayout`            | `({ size: Int32 }) => void` | Optional | Called when a batch of layout calculations is complete. |
| `onEndReached`             | `({ distanceFromEnd: Int32 }) => void` | Optional | Called when the end of the content is within `onEndReachedThreshold`. |
| `onEndReachedThreshold`    | Double                    | Optional | The threshold (in content length units) at which `onEndReached` is triggered. |

## Methods
| Method          | Type                                | Description                                               |
|-----------------|-------------------------------------|-----------------------------------------------------------|
| `scrollToIndex` | `({ index: number; animated: boolean }) => void` | Scrolls the list to the specified index.                  |
| `scrollToOffset`| `({ offset: number; animated: boolean }) => void` | Scrolls the list to the specified offset.                 |

## Contributing

See the [contributing guide](CONTRIBUTING.md) to learn how to contribute to the repository and the development workflow.

## License

MIT

