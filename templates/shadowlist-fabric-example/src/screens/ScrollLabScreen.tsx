import { useCallback, useMemo, useRef, useState } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import type {
  AnchorState,
  ScrollEvent,
  ScrollToIndexFailedInfo,
  ShadowListCommands,
  ViewabilityConfigCallbackPair,
} from 'shadowlist';
import { ShadowList } from 'shadowlist';
import { getViewableRange } from 'shadowlist-utils';
import {
  ItemSeparator,
  Segmented,
  TextField,
  createStyles,
} from 'shadowlist-utils/native';
import { useHeaderMenu } from './HeaderActions';
import { DEBUG } from '../launchSettings';

interface LabRow {
  id: string;
  index: number;
  lines: number;
}

const ROW_COUNT = 500;
const INITIAL_INDEX = 120;

const ROWS: LabRow[] = Array.from({ length: ROW_COUNT }, (_, index) => ({
  id: `lab-${index}`,
  index,
  lines: 1 + ((index * 7) % 4),
}));

const FILLER =
  'Rows have different heights so scroll commands land through estimated sizes.';

type Columns = '1' | '2';
type Padding = 'none' | 'padded';

const COLUMN_OPTIONS = [
  { id: '1' as const, label: 'List' },
  { id: '2' as const, label: 'Grid' },
];
const PADDING_OPTIONS = [
  { id: 'none' as const, label: 'No Padding' },
  { id: 'padded' as const, label: 'Padding' },
];

const PADDED_CONTENT = {
  paddingTop: 24,
  paddingBottom: 48,
  paddingHorizontal: 16,
};
const COLUMN_WRAPPER = { columnGap: 12 };

function describeScroll(name: string, event: ScrollEvent): string {
  const { contentOffset, contentSize, layoutMeasurement, velocity } =
    event.nativeEvent;
  const speed = velocity ? ` v=${velocity.y.toFixed(2)}` : '';
  return (
    `${name} y=${contentOffset.y.toFixed(0)}/${contentSize.height.toFixed(0)}` +
    ` h=${layoutMeasurement.height.toFixed(0)}${speed}`
  );
}

/*
 * The list's scroll commands, scroll events and viewability in one place. It opens at row 120
 * through initialScrollIndex. The More menu jumps, saves and restores the position and switches
 * layouts. Two viewability configs report at once.
 */
export const ScrollLabScreen = () => {
  const styles = useStyles();
  const listRef = useRef<ShadowListCommands>(null);

  const [columns, setColumns] = useState<Columns>('1');
  const [padding, setPadding] = useState<Padding>('none');
  const [scrollEnabled, setScrollEnabled] = useState(true);
  const [marked, setMarked] = useState(7);
  const [query, setQuery] = useState('');
  const [anchor, setAnchor] = useState<AnchorState | null>(null);

  const [scrollLine, setScrollLine] = useState('—');
  const [phaseLine, setPhaseLine] = useState('—');
  const [sizeLine, setSizeLine] = useState('—');
  const [viewableLine, setViewableLine] = useState('—');
  const [coveredLine, setCoveredLine] = useState('—');
  const [commandLine, setCommandLine] = useState('—');
  const [pressedLine, setPressedLine] = useState('—');

  const scrollTo = useCallback(
    (index: number, viewPosition: number, viewOffset = 0) => {
      setCommandLine(`to ${index} pos=${viewPosition} off=${viewOffset}`);
      listRef.current?.scrollToIndex({
        index,
        viewPosition,
        viewOffset,
        animated: true,
      });
    },
    []
  );

  const handleScrollToIndexFailed = useCallback(
    (info: ScrollToIndexFailedInfo) => {
      setCommandLine(
        `failed ${info.index} measured=${info.highestMeasuredFrameIndex}` +
          ` avg=${info.averageItemLength.toFixed(0)}`
      );
    },
    []
  );

  useHeaderMenu([
    [
      {
        label: 'Scroll to Row 300 (Top)',
        symbol: 'arrow.up.to.line',
        onPress: () => scrollTo(300, 0),
      },
      {
        label: 'Scroll to Row 40 (Center)',
        symbol: 'arrow.up.and.down',
        onPress: () => scrollTo(40, 0.5),
      },
      {
        label: 'Scroll to Row 450 (Bottom, 20 pt)',
        symbol: 'arrow.down.to.line',
        onPress: () => scrollTo(450, 1, 20),
      },
      {
        label: 'Scroll to Row 9999',
        symbol: 'exclamationmark.triangle',
        onPress: () => scrollTo(9999, 0),
      },
    ],
    [
      {
        label: 'Scroll to Start',
        symbol: 'arrow.up',
        onPress: () => {
          setCommandLine('offset 0');
          listRef.current?.scrollToOffset({ offset: 0, animated: true });
        },
      },
      {
        label: 'Scroll to End',
        symbol: 'arrow.down',
        onPress: () => {
          setCommandLine('end');
          listRef.current?.scrollToEnd();
        },
      },
      {
        label: 'Flash Scroll Indicators',
        symbol: 'sparkles',
        onPress: () => {
          listRef.current?.flashScrollIndicators();
          setCommandLine(
            `flash node=${listRef.current?.getScrollableNode() ?? 'null'}`
          );
        },
      },
    ],
    [
      {
        label: 'Save Position',
        symbol: 'bookmark',
        onPress: () => {
          listRef.current?.getAnchorState().then((state) => {
            setAnchor(state);
            setCommandLine(
              state
                ? `saved ${state.key}@${state.offset.toFixed(1)}`
                : 'saved none'
            );
          });
        },
      },
      {
        label: 'Restore Position',
        symbol: 'bookmark.fill',
        onPress: () => {
          if (!anchor) return;
          setCommandLine(`restore ${anchor.key}@${anchor.offset.toFixed(1)}`);
          listRef.current?.restoreAnchorState(anchor);
        },
      },
    ],
    [
      {
        label: 'Scrolling Enabled',
        symbol: 'hand.draw',
        checked: scrollEnabled,
        onPress: () => setScrollEnabled((enabled) => !enabled),
      },
      {
        label: `Mark Every ${marked === 7 ? 5 : 7}th Row`,
        symbol: 'number',
        onPress: () => setMarked((every) => (every === 7 ? 5 : 7)),
      },
    ],
  ]);

  /*
   * Two viewability configs at once: rows at least half visible, and rows covering a tenth of
   * the viewport for 250 ms after the first interaction.
   */
  const viewabilityPairs = useMemo<ViewabilityConfigCallbackPair<LabRow>[]>(
    () => [
      {
        viewabilityConfig: { itemVisiblePercentThreshold: 50 },
        onViewableItemsChanged: ({ viewableItems, changed }) => {
          const range = getViewableRange(viewableItems);
          setViewableLine(
            range
              ? `${range.firstIndex}–${range.lastIndex} (+${changed.filter((token) => token.isViewable).length})`
              : '—'
          );
        },
      },
      {
        viewabilityConfig: {
          viewAreaCoveragePercentThreshold: 10,
          minimumViewTime: 250,
          waitForInteraction: true,
        },
        onViewableItemsChanged: ({ viewableItems }) => {
          const range = getViewableRange(viewableItems);
          setCoveredLine(
            range ? `${range.firstIndex}–${range.lastIndex}` : '—'
          );
        },
      },
    ],
    []
  );

  const renderElement = useCallback(
    ({ element }: { element: LabRow }) => (
      <Pressable
        onPress={() => setPressedLine(`row ${element.index}`)}
        style={({ pressed }) => [
          styles.row,
          element.index % marked === 0 && styles.marked,
          pressed && styles.pressed,
        ]}
        accessibilityRole="button"
        accessibilityLabel={`Row ${element.index}`}
      >
        <Text style={styles.rowTitle}>{`Row ${element.index}`}</Text>
        <Text style={styles.rowText} numberOfLines={element.lines}>
          {Array.from({ length: element.lines }, () => FILLER).join(' ')}
        </Text>
      </Pressable>
    ),
    [styles, marked]
  );

  const header = (
    <View>
      <Segmented
        options={COLUMN_OPTIONS}
        value={columns}
        onChange={setColumns}
        size="compact"
        accessibilityLabel="Layout"
      />
      <Segmented
        options={PADDING_OPTIONS}
        value={padding}
        onChange={setPadding}
        size="compact"
        accessibilityLabel="Content padding"
        style={styles.headerGap}
      />
      <TextField
        value={query}
        onChange={setQuery}
        placeholder="Type, then tap a row"
        style={styles.headerGap}
      />
    </View>
  );

  return (
    <View style={styles.container}>
      {DEBUG ? (
        <View style={styles.status} testID="lab-status">
          <Text style={styles.statusText}>{`scroll ${scrollLine}`}</Text>
          <Text
            style={styles.statusText}
          >{`phase ${phaseLine} size ${sizeLine}`}</Text>
          <Text style={styles.statusText}>
            {`viewable ${viewableLine} covered ${coveredLine}`}
          </Text>
          <Text style={styles.statusText}>
            {`command ${commandLine} pressed ${pressedLine}`}
          </Text>
        </View>
      ) : null}
      <ShadowList
        key={columns}
        ref={listRef}
        data={ROWS}
        style={styles.list}
        renderElement={renderElement}
        extraData={marked}
        numberOfColumns={Number(columns)}
        columnWrapperStyle={columns === '2' ? COLUMN_WRAPPER : undefined}
        contentContainerStyle={
          padding === 'padded' ? PADDED_CONTENT : undefined
        }
        ItemSeparatorComponent={columns === '1' ? ItemSeparator : null}
        initialScrollIndex={INITIAL_INDEX}
        onScrollToIndexFailed={handleScrollToIndexFailed}
        scrollEnabled={scrollEnabled}
        keyboardShouldPersistTaps="handled"
        keyboardDismissMode="on-drag"
        decelerationRate="normal"
        scrollEventThrottle={100}
        onScroll={(event) => setScrollLine(describeScroll('', event))}
        onScrollBeginDrag={(event) =>
          setPhaseLine(describeScroll('drag', event))
        }
        onScrollEndDrag={(event) =>
          setPhaseLine(describeScroll('release', event))
        }
        onMomentumScrollBegin={(event) =>
          setPhaseLine(describeScroll('glide', event))
        }
        onMomentumScrollEnd={(event) =>
          setPhaseLine(describeScroll('rest', event))
        }
        onContentSizeChange={(width, height) =>
          setSizeLine(`${width.toFixed(0)}x${height.toFixed(0)}`)
        }
        viewabilityConfigCallbackPairs={viewabilityPairs}
        ListHeaderComponent={header}
        ListHeaderComponentStyle={styles.header}
      />
    </View>
  );
};

const useStyles = createStyles(({ colors, spacing, typography, radius }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
    header: {
      padding: spacing.lg,
      backgroundColor: colors.groupedBackground,
    },
    headerGap: {
      marginTop: spacing.sm,
    },
    status: {
      paddingHorizontal: spacing.lg,
      paddingVertical: spacing.xs,
      backgroundColor: colors.groupedBackground,
    },
    statusText: {
      ...typography.caption,
      color: colors.secondaryLabel,
    },
    row: {
      paddingHorizontal: spacing.lg,
      paddingVertical: spacing.md,
      backgroundColor: colors.background,
      borderRadius: radius.sm,
    },
    marked: {
      backgroundColor: colors.accentSoft,
    },
    pressed: {
      backgroundColor: colors.fill,
    },
    rowTitle: {
      ...typography.headline,
      color: colors.label,
    },
    rowText: {
      ...typography.subhead,
      color: colors.secondaryLabel,
    },
  })
);
