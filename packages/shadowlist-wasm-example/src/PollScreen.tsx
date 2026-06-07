import {
  useState,
  useRef,
  useCallback,
  useMemo,
  type CSSProperties,
} from 'react';
import { type ShadowlistCommands } from 'shadowlist-wasm';
import {
  Poll,
  ListHeader,
  colors,
  typography,
  buildOption,
  buildPoll,
  type PollOption,
} from 'shadowlist-utils/web';
import { useHeaderActions } from './HeaderActions';

/*
 * Poll — a short interactive list combining a sticky header, a sticky footer
 * (the live tally), pull-to-refresh and per-row state updates. Clicking an
 * option casts a vote; the bars and the leading highlight update live.
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

  // Pull-to-refresh stands in for loading a fresh poll.
  const [refreshing, setRefreshing] = useState(false);
  const handleRefresh = useCallback(() => {
    setRefreshing(true);
    window.setTimeout(() => {
      setData(buildPoll());
      setRefreshing(false);
    }, 900);
  }, []);

  const footer = useMemo(
    () => (
      <div style={styles.footer}>
        <span style={styles.footerText}>
          {`${total} votes · tap to vote · pull to reset`}
        </span>
      </div>
    ),
    [total]
  );

  return (
    <div style={styles.container}>
      <Poll.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={({ element }) => (
          <Poll.Option
            option={element}
            total={total}
            leading={element.id === leadingId}
            onVote={handleVote}
          />
        )}
        refreshing={refreshing}
        onRefresh={handleRefresh}
        refreshColor={colors.secondaryLabel}
        ListHeaderComponent={
          <ListHeader title="Poll" subtitle="Short interactive list" />
        }
        ListFooterComponent={footer}
      />
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  footer: {
    width: '100%',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    background: colors.background,
    padding: 16,
    borderTop: `1px solid ${colors.separator}`,
    boxSizing: 'border-box',
  },
  footerText: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
};
