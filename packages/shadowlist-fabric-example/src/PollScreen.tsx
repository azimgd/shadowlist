import { useState, useRef, useCallback, useMemo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { type ShadowlistCommands } from 'shadowlist';
import {
  Poll,
  ListHeader,
  colors,
  typography,
  buildOption,
  buildPoll,
  type PollOption,
} from 'shadowlist-utils/native';
import { useHeaderActions } from './HeaderActions';

/*
 * Poll — a deliberately SHORT (5-item) interactive list that exercises a combination of
 * features at once: a sticky header (the question), a sticky full-width footer (the live
 * tally) pinned over content that never fills the viewport, pull-to-refresh (load a fresh
 * poll) and per-row state updates. Tapping an option casts a vote; the result bars and the
 * leading highlight update live.
 */

export const PollScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<PollOption[]>(() => buildPoll());

  const total = useMemo(
    () => data.reduce((sum, option) => sum + option.votes, 0),
    [data]
  );
  const leadingId = useMemo(
    () =>
      data.length === 0
        ? undefined
        : data.reduce(
            (best, option) => (option.votes > best.votes ? option : best),
            data[0]!
          ).id,
    [data]
  );

  const handleVote = useCallback((id: string) => {
    setData((prev) =>
      prev.map((option) =>
        option.id === id ? { ...option, votes: option.votes + 1 } : option
      )
    );
  }, []);

  // Pull-to-refresh stands in for loading a fresh poll.
  const [refreshing, setRefreshing] = useState(false);
  const handleRefresh = useCallback(() => {
    setRefreshing(true);
    setTimeout(() => {
      setData(buildPoll());
      setRefreshing(false);
    }, 900);
  }, []);

  // List controls in the nav bar (prepend / append / scroll-to-random), as on Feed.
  const handlePrepend = () => {
    const currentLength = data.length;
    setData((prev) => [
      ...Array.from({ length: 3 }, (_, index) =>
        buildOption(currentLength + index)
      ),
      ...prev,
    ]);
  };
  const handleAppend = () => {
    const currentLength = data.length;
    setData((prev) => [
      ...prev,
      ...Array.from({ length: 3 }, (_, index) =>
        buildOption(currentLength + index)
      ),
    ]);
  };
  const handleScrollToRandom = () => {
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * data.length)
    );
  };
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
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={renderElement}
        refreshing={refreshing}
        onRefresh={handleRefresh}
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
