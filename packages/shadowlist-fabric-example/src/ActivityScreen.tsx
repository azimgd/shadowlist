import { useState, useRef, useCallback, useMemo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import { getViewableRange, useInfiniteListProps } from 'shadowlist-utils';
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
} from './fixtures/activity';
import { useHeaderMenu } from './HeaderActions';
import { DEBUG } from './launchSettings';
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

  useHeaderMenu([
    [
      {
        label: 'Scroll Down 2,000 pt',
        symbol: 'arrow.down',
        onPress: () => shadowlistRef.current?.scrollToOffset(2000),
      },
      {
        label: 'Scroll to End',
        symbol: 'arrow.down.to.line',
        onPress: () => shadowlistRef.current?.scrollToEnd(),
      },
    ],
    [
      {
        label: `Start Threshold: ${startThreshold}×`,
        symbol: 'arrow.up.and.down',
        onPress: () =>
          setStartThreshold((previous) =>
            nextInCycle(START_REACHED_THRESHOLDS, previous)
          ),
      },
      {
        label: `End Threshold: ${endThreshold}×`,
        symbol: 'arrow.up.and.down',
        onPress: () =>
          setEndThreshold((previous) =>
            nextInCycle(END_REACHED_THRESHOLDS, previous)
          ),
      },
    ],
    [
      {
        label: 'Remove Items 20 and 50',
        symbol: 'trash',
        destructive: true,
        onPress: handleRemoveItems,
      },
    ],
  ]);

  const { isFetchingNextPage } = activity;
  const footer = useMemo(
    () => (
      <View style={styles.statusFooter}>
        {DEBUG ? (
          <Text style={styles.statusText}>
            {`Viewable: ${viewableLabel} · Total: ${list.data.length}`}
          </Text>
        ) : null}
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
        refreshing={list.refreshing}
        onRefresh={list.onRefresh}
        refreshColor={colors.secondaryLabel}
        ListFooterComponent={footer}
        onEndReached={list.onEndReached}
        onStartReachedThreshold={startThreshold}
        onEndReachedThreshold={endThreshold}
        onViewableItemsChanged={DEBUG ? handleViewableItemsChanged : undefined}
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
