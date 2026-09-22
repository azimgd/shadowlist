import { useState, useRef, useCallback, useMemo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import {
  getViewableRange,
  useInfiniteListProps,
  useScrollThreshold,
} from 'shadowlist-utils';
import {
  Activity,
  Spinner,
  createStyles,
  useTheme,
} from 'shadowlist-utils/native';
import {
  START_REACHED_THRESHOLDS,
  END_REACHED_THRESHOLDS,
  nextInCycle,
  HEADER_HIDE_THRESHOLD,
} from './fixtures/activity';
import { QueryStatus } from './QueryStatus';
import {
  useActivityQuery,
  useDeleteActivities,
  useRefreshActivity,
} from './queries/activity';

export const ActivityScreen = () => {
  const styles = useStyles();
  const { colors } = useTheme();
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const activity = useActivityQuery();
  const refreshActivity = useRefreshActivity();
  const list = useInfiniteListProps(activity, { refresh: refreshActivity });
  const { mutate: deleteActivities } = useDeleteActivities();

  const [viewableLabel, setViewableLabel] = useState('—');
  const [startThreshold, setStartThreshold] = useState(1);
  const [endThreshold, setEndThreshold] = useState(1.5);

  const { isPastThreshold: headerHidden, onScroll } = useScrollThreshold(
    HEADER_HIDE_THRESHOLD
  );

  const handleViewableItemsChanged = useCallback(
    ({ viewableItems }: { viewableItems: { index: number }[] }) => {
      const range = getViewableRange(viewableItems);
      setViewableLabel(range ? `${range.firstIndex}–${range.lastIndex}` : '—');
    },
    []
  );

  const dataRef = useRef(list.data);
  dataRef.current = list.data;

  const handleRemoveItems = useCallback(() => {
    const ids = [20, 50].flatMap((index) => {
      const row = dataRef.current[index];
      return row ? [row.id] : [];
    });
    deleteActivities(ids);
  }, [deleteActivities]);

  const header = useMemo(
    () => (
      <Activity.Header
        title="Activity"
        subtitle="Boarding calls, likes and new followers, opens at index 30"
        actions={[
          {
            label: 'Offset 2000',
            onPress: () => shadowlistRef.current?.scrollToOffset(2000),
          },
          {
            label: 'Scroll to end',
            onPress: () => shadowlistRef.current?.scrollToEnd(),
          },
          {
            label: `Start ×${startThreshold}`,
            onPress: () =>
              setStartThreshold((previous) =>
                nextInCycle(START_REACHED_THRESHOLDS, previous)
              ),
          },
          {
            label: `End ×${endThreshold}`,
            onPress: () =>
              setEndThreshold((previous) =>
                nextInCycle(END_REACHED_THRESHOLDS, previous)
              ),
          },
          { label: 'Remove 20 & 50', onPress: handleRemoveItems },
        ]}
      />
    ),
    [startThreshold, endThreshold, handleRemoveItems]
  );

  const { isFetchingNextPage } = activity;
  const footer = useMemo(
    () => (
      <View style={styles.statusFooter}>
        <Text style={styles.statusText}>
          {`Viewable: ${viewableLabel} · Total: ${list.data.length}`}
        </Text>
        {isFetchingNextPage && <Spinner size={16} />}
      </View>
    ),
    [styles, isFetchingNextPage, viewableLabel, list.data.length]
  );

  // Mount the list with its first page so containerOffsetIndex has rows to land on.
  if (activity.data === undefined) {
    return <QueryStatus error={activity.error} onRetry={activity.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Activity.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        containerOffsetIndex={30}
        stickyHeader={!headerHidden}
        refreshing={list.refreshing}
        onRefresh={list.onRefresh}
        refreshColor={colors.secondaryLabel}
        ListHeaderComponent={header}
        ListFooterComponent={footer}
        onScroll={onScroll}
        onEndReached={list.onEndReached}
        onStartReachedThreshold={startThreshold}
        onEndReachedThreshold={endThreshold}
        onViewableItemsChanged={handleViewableItemsChanged}
      />
    </View>
  );
};

const useStyles = createStyles(({ colors, typography }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
    statusFooter: {
      width: '100%',
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'center',
      gap: 8,
      backgroundColor: colors.background,
      paddingHorizontal: 16,
      paddingVertical: 20,
    },
    statusText: {
      color: colors.secondaryLabel,
      ...typography.footnote,
    },
  })
);
