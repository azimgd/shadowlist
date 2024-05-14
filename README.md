# react-native-shadow-list (alpha release)

ShadowList is a new alternative to FlatList for React Native, created to address common performance issues and enhance the UX when dealing with large lists of data. It's built on Fabric and works with React Native version 0.74 and newer.

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

## Performance
| Number of Items | ShadowList Speed | FlatList Speed |
|-----------------|------------------|----------------|
| 100 (text only) | 10% faster       |                |
| 1000 (text only)| 50% faster       |                |

## Installation
```sh
npm install react-native-shadow-list
```

## Usage

```js
import { ShadowListContainer } from "react-native-shadow-list";

<ShadowListContainer
  style={styles.container}
  ref={shadowListContainerRef}
  data={data}
  ListHeaderComponent={ListHeaderComponent}
  ListFooterComponent={ListFooterComponent}
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
| `renderItem`           | Function                 | Required | A function to render each item in the list. It receives an object with `item` and `index` properties. |
| `initialScrollIndex`   | Number                   | Optional | The initial index of the item to scroll to when the list mounts. |
| `inverted`             | Boolean                  | Optional | If true, the list will be rendered in an inverted order. |

## Contributing

See the [contributing guide](CONTRIBUTING.md) to learn how to contribute to the repository and the development workflow.

## License

MIT
