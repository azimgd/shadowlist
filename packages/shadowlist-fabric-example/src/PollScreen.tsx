import { useRef, useCallback, useMemo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import {
  Poll,
  ListHeader,
  colors,
  typography,
  buildOption,
  buildPoll,
  type PollOption,
} from 'shadowlist-utils/native';
import { useListController } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

/*
 * A deliberately SHORT (5-item) interactive list that exercises a combination of
 * features at once: a sticky header (the question), a sticky full-width footer (the live
 * tally) pinned over content that never fills the viewport, pull-to-refresh (load a fresh
 * poll) and per-row state updates. Tapping an option casts a vote; the result bars and the
 * leading highlight update live.
 */

export const PollScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const initialData = useMemo(() => buildPoll(), []);

  // Pull-to-refresh stands in for loading a fresh poll.
  const list = useListController<PollOption>({
    initialData,
    onRefresh: () =>
      new Promise<void>((resolve) =>
        setTimeout(() => {
          list.setData(buildPoll());
          resolve();
        }, 900)
      ),
  });
  const { setData } = list;

  const total = useMemo(
    () => list.data.reduce((sum, option) => sum + option.votes, 0),
    [list.data]
  );
  const leadingId = useMemo(
    () =>
      list.data.length === 0
        ? undefined
        : list.data.reduce(
            (best, option) => (option.votes > best.votes ? option : best),
            list.data[0]!
          ).id,
    [list.data]
  );

  const handleVote = useCallback(
    (key: string) =>
      setData((prev) =>
        prev.map((option) =>
          option.id === key ? { ...option, votes: option.votes + 1 } : option
        )
      ),
    [setData]
  );

  // List controls in the nav bar (prepend / append / scroll-to-random), as on Feed.
  const handlePrepend = () =>
    list.prepend(
      Array.from({ length: 3 }, (_, index) =>
        buildOption(list.data.length + index)
      )
    );
  const handleAppend = () =>
    list.append(
      Array.from({ length: 3 }, (_, index) =>
        buildOption(list.data.length + index)
      )
    );
  const handleScrollToRandom = () =>
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * list.data.length)
    );
  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  const footer = useMemo(
    () => (
      <View style={styles.footer}>
        <Text style={styles.footerText}>
          {`${total} votes · tap to vote · pull to reset`}
        </Text>
      </View>
    ),
    [total]
  );

  const renderElement = useCallback(
    ({ element }: { element: PollOption }) => (
      <Poll.Option
        option={element}
        total={total}
        leading={element.id === leadingId}
        onVote={handleVote}
      />
    ),
    [total, leadingId, handleVote]
  );

  return (
    <View style={styles.container}>
      <Poll.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={renderElement}
        refreshing={list.refreshing}
        onRefresh={list.handleRefresh}
        refreshColor={colors.secondaryLabel}
        ListHeaderComponent={
          <ListHeader title="Poll" subtitle="Short interactive list" />
        }
        ListFooterComponent={footer}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: colors.background,
  },
  list: {
    flex: 1,
    backgroundColor: colors.background,
  },
  footer: {
    width: '100%',
    alignItems: 'center',
    backgroundColor: colors.background,
    paddingHorizontal: 16,
    paddingVertical: 16,
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: colors.separator,
  },
  footerText: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
});
