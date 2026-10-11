import { useCallback, useMemo, useRef, useState } from 'react';
import { StyleSheet, Text, View } from 'react-native';
import type {
  AnchorState,
  PrefetchDataSource,
  ShadowListCommands,
} from 'shadowlist';
import {
  Inbox,
  ListFooter,
  PillButton,
  createStyles,
  useTheme,
  type InboxMessage,
} from 'shadowlist-utils/native';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { DEBUG } from '../launchSettings';
import {
  useDeleteMessages,
  useInboxQuery,
  useReceiveMail,
  useUpdateMessages,
} from '../queries/inbox';

const NO_KEYS: ReadonlyArray<string> = [];

/*
 * A mailbox on the list's interaction features: native swipe actions with full swipe, a context
 * menu, selection while editing, separators that hide next to a pressed row, prefetching, pull to
 * refresh and a saved scroll position.
 */
export const InboxScreen = () => {
  const styles = useStyles();
  const { colors } = useTheme();
  const listRef = useRef<ShadowListCommands>(null);

  const inbox = useInboxQuery();
  const { mutateAsync: receiveMail } = useReceiveMail();
  const { mutateAsync: updateMessages } = useUpdateMessages();
  const { mutateAsync: deleteMessages } = useDeleteMessages();

  const [editing, setEditing] = useState(false);
  const [selectedKeys, setSelectedKeys] =
    useState<ReadonlyArray<string>>(NO_KEYS);
  const [refreshing, setRefreshing] = useState(false);
  const [savedAnchor, setSavedAnchor] = useState<AnchorState | null>(null);
  const [lastAction, setLastAction] = useState('—');
  const [prefetchCounts, setPrefetchCounts] = useState({
    prefetched: 0,
    cancelled: 0,
  });

  const toggleRead = useCallback(
    (item: InboxMessage) => {
      setLastAction(`${item.read ? 'unread' : 'read'} ${item.sender}`);
      return updateMessages({ ids: [item.id], change: { read: !item.read } });
    },
    [updateMessages]
  );
  const toggleFlag = useCallback(
    (item: InboxMessage) => {
      setLastAction(`${item.flagged ? 'unflag' : 'flag'} ${item.sender}`);
      return updateMessages({
        ids: [item.id],
        change: { flagged: !item.flagged },
      });
    },
    [updateMessages]
  );
  const deleteMessage = useCallback(
    (item: InboxMessage) => {
      setLastAction(`delete ${item.sender}`);
      return deleteMessages([item.id]);
    },
    [deleteMessages]
  );
  const reply = useCallback((item: InboxMessage) => {
    setLastAction(`reply ${item.sender}`);
  }, []);
  const openMessage = useCallback(
    (item: InboxMessage) => {
      setLastAction(`open ${item.sender}`);
      if (!item.read) {
        updateMessages({ ids: [item.id], change: { read: true } });
      }
    },
    [updateMessages]
  );
  const startSelecting = useCallback((item: InboxMessage) => {
    setEditing(true);
    setSelectedKeys([item.id]);
  }, []);
  const stopEditing = useCallback(() => {
    setEditing(false);
    setSelectedKeys(NO_KEYS);
  }, []);

  const handleRefresh = useCallback(async () => {
    setRefreshing(true);
    try {
      await receiveMail(3);
    } finally {
      setRefreshing(false);
    }
  }, [receiveMail]);

  /*
   * Rows near the screen are announced before they mount. A real app would start loading their
   * avatars or bodies here.
   */
  const prefetchDataSource = useMemo<PrefetchDataSource>(
    () => ({
      prefetchItems: (indices) =>
        setPrefetchCounts((counts) => ({
          ...counts,
          prefetched: counts.prefetched + indices.length,
        })),
      cancelPrefetchingForItems: (indices) =>
        setPrefetchCounts((counts) => ({
          ...counts,
          cancelled: counts.cancelled + indices.length,
        })),
    }),
    []
  );

  const data = inbox.data;
  const dataRef = useRef(data);
  dataRef.current = data;

  const markSelected = useCallback(
    (read: boolean) => {
      updateMessages({ ids: selectedKeys, change: { read } });
      stopEditing();
    },
    [selectedKeys, updateMessages, stopEditing]
  );
  const deleteSelected = useCallback(() => {
    deleteMessages(selectedKeys);
    stopEditing();
  }, [selectedKeys, deleteMessages, stopEditing]);

  useHeaderActions(
    {
      onPrepend: () => receiveMail(5),
      onAppend: () => listRef.current?.scrollToEnd({ animated: true }),
      onScrollToRandom: () => {
        const count = dataRef.current?.length ?? 0;
        listRef.current?.scrollToIndex({
          index: Math.floor(Math.random() * count),
          viewPosition: 0.5,
          animated: true,
        });
      },
      prependLabel: 'Receive New Mail',
      appendLabel: 'Scroll to Oldest',
    },
    [
      editing
        ? [
            {
              label: 'Done',
              symbol: 'checkmark',
              onPress: stopEditing,
            },
            {
              label: 'Select All',
              symbol: 'checkmark.circle.fill',
              onPress: () =>
                setSelectedKeys(
                  dataRef.current?.map((item) => item.id) ?? NO_KEYS
                ),
            },
          ]
        : [
            {
              label: 'Select Messages',
              symbol: 'checkmark.circle',
              onPress: () => setEditing(true),
            },
          ],
      [
        {
          label: 'Save Position',
          symbol: 'bookmark',
          onPress: () => {
            listRef.current?.getAnchorState().then((state) => {
              setSavedAnchor(state);
              setLastAction(
                state
                  ? `saved ${state.key.slice(-4)}@${state.offset.toFixed(0)}`
                  : 'save failed'
              );
            });
          },
        },
        {
          label: 'Restore Position',
          symbol: 'bookmark.fill',
          onPress: () => {
            if (savedAnchor) listRef.current?.restoreAnchorState(savedAnchor);
          },
        },
        {
          label: 'Close Swipe Actions',
          symbol: 'xmark',
          onPress: () => listRef.current?.closeSwipeActions(),
        },
      ],
    ]
  );

  if (data === undefined) {
    return <QueryStatus error={inbox.error} onRetry={inbox.refetch} />;
  }

  const unread = data.reduce((count, item) => count + (item.read ? 0 : 1), 0);

  return (
    <View style={styles.container}>
      {DEBUG ? (
        <Text style={styles.status} testID="inbox-status">
          {`sel=${selectedKeys.length} unread=${unread} n=${data.length}` +
            ` prefetch=${prefetchCounts.prefetched}/${prefetchCounts.cancelled}` +
            ` last=${lastAction}`}
        </Text>
      ) : null}
      <Inbox.List
        ref={listRef}
        data={data}
        style={styles.list}
        editing={editing}
        selectedKeys={selectedKeys}
        onSelectionChange={setSelectedKeys}
        onPressItem={openMessage}
        onToggleRead={toggleRead}
        onToggleFlag={toggleFlag}
        onDelete={deleteMessage}
        onReply={reply}
        onSelectItem={startSelecting}
        prefetchDataSource={prefetchDataSource}
        refreshing={refreshing}
        onRefresh={handleRefresh}
        refreshColor={colors.secondaryLabel}
        ListFooterComponent={
          <ListFooter text={`${data.length} messages, ${unread} unread`} />
        }
        ListEmptyComponent={
          <Text style={styles.empty} testID="inbox-empty">
            No messages
          </Text>
        }
      />
      {editing ? (
        <View style={styles.toolbar}>
          <PillButton
            label="Mark Read"
            variant="tinted"
            disabled={selectedKeys.length === 0}
            onPress={() => markSelected(true)}
          />
          <PillButton
            label={`Delete (${selectedKeys.length})`}
            variant="destructive"
            disabled={selectedKeys.length === 0}
            onPress={deleteSelected}
          />
        </View>
      ) : null}
    </View>
  );
};

const useStyles = createStyles(({ colors, spacing, typography }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
    status: {
      ...typography.caption,
      color: colors.secondaryLabel,
      paddingHorizontal: spacing.lg,
      paddingVertical: spacing.xs,
    },
    empty: {
      ...typography.body,
      color: colors.secondaryLabel,
      textAlign: 'center',
      paddingVertical: spacing.xl,
    },
    toolbar: {
      flexDirection: 'row',
      justifyContent: 'space-between',
      paddingHorizontal: spacing.lg,
      paddingTop: spacing.sm,
      paddingBottom: spacing.xl,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
      backgroundColor: colors.background,
    },
  })
);
