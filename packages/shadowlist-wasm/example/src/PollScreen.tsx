import {
  useState,
  useRef,
  useCallback,
  useMemo,
  memo,
  type ComponentType,
  type CSSProperties,
} from 'react';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist-wasm';
import { useHeaderActions } from './HeaderActions';
import { Bell, CircleSlash, Globe, HalfCircle, Swatch } from './icons';
import { generateUniqueId } from './constants';
import { HeaderListItem } from './HeaderListItem';
import { colors, typography, radius } from './theme';

/*
 * Poll — a deliberately SHORT (5-item) interactive list that exercises a combination of
 * features at once: a sticky header, a sticky full-width footer (the live tally) pinned
 * over content that never fills the viewport, pull-to-refresh (load a fresh poll) and
 * per-row state updates. Clicking an option casts a vote; the result bars and the leading
 * highlight update live.
 */

type IconComponent = ComponentType<{ size?: number; color?: string }>;

type PollOption = {
  id: string;
  Icon: IconComponent;
  label: string;
  votes: number;
};

const OPTION_SEEDS: { Icon: IconComponent; label: string }[] = [
  { Icon: HalfCircle, label: 'Dark mode' },
  { Icon: CircleSlash, label: 'Offline mode' },
  { Icon: Bell, label: 'Notifications' },
  { Icon: Globe, label: 'Translations' },
  { Icon: Swatch, label: 'Custom themes' },
];

// One option, cycling the seed set; a fresh id keeps prepend/append unique.
const buildOption = (index: number): PollOption => {
  const seed = OPTION_SEEDS[index % OPTION_SEEDS.length]!;
  return {
    id: generateUniqueId(),
    Icon: seed.Icon,
    label: seed.label,
    votes: 5 + Math.floor(Math.random() * 40),
  };
};

const buildPoll = (): PollOption[] =>
  Array.from({ length: 5 }, (_, index) => buildOption(index));

// One option: icon chip, label, its share, and an inline result bar. The current leader
// is tinted with the app accent. Clicking anywhere on the row casts a vote.
const PollOptionRow = memo(
  ({
    option,
    total,
    leading,
    onVote,
  }: {
    option: PollOption;
    total: number;
    leading: boolean;
    onVote: (id: string) => void;
  }) => {
    const share = total > 0 ? option.votes / total : 0;
    const percent = Math.round(share * 100);
    const Icon = option.Icon;
    return (
      <div style={styles.row} onClick={() => onVote(option.id)}>
        <div
          style={{
            ...styles.iconChip,
            ...(leading ? styles.iconChipLeading : null),
          }}
        >
          <Icon size={22} color={leading ? colors.accent : colors.label} />
        </div>
        <div style={styles.body}>
          <div style={styles.bodyTop}>
            <span style={styles.label}>{option.label}</span>
            <span
              style={{
                ...styles.share,
                ...(leading ? styles.shareLeading : null),
              }}
            >
              {percent}%
            </span>
          </div>
          <div style={styles.barTrack}>
            <div
              style={{
                ...styles.barFill,
                ...(leading ? styles.barFillLeading : null),
                width: `${percent}%`,
              }}
            />
          </div>
        </div>
      </div>
    );
  }
);

export const PollScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<PollOption[]>(buildPoll);

  const total = useMemo(
    () => data.reduce((sum, option) => sum + option.votes, 0),
    [data]
  );
  const leadingId = useMemo(
    () =>
      data.reduce(
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
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        keyExtractor={(item) => item.id}
        renderElement={({ element }) => (
          <PollOptionRow
            option={element}
            total={total}
            leading={element.id === leadingId}
            onVote={handleVote}
          />
        )}
        stickyHeader
        stickyFooter
        refreshing={refreshing}
        onRefresh={handleRefresh}
        refreshColor={colors.secondaryLabel}
        ListHeaderComponent={
          <HeaderListItem title="Poll" subtitle="Short interactive list" />
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
  row: {
    display: 'flex',
    alignItems: 'center',
    padding: '14px 16px',
    background: colors.background,
    boxSizing: 'border-box',
    cursor: 'pointer',
  },
  iconChip: {
    width: 44,
    height: 44,
    borderRadius: radius.pill,
    background: colors.elevated,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 14,
    flexShrink: 0,
  },
  iconChipLeading: {
    background: colors.accentSoft,
  },
  body: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
  },
  bodyTop: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  label: {
    color: colors.label,
    ...typography.headline,
  },
  share: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  shareLeading: {
    color: colors.accent,
    fontWeight: 600,
  },
  barTrack: {
    height: 6,
    borderRadius: radius.pill,
    background: colors.fill,
    overflow: 'hidden',
  },
  barFill: {
    height: '100%',
    borderRadius: radius.pill,
    background: colors.secondaryLabel,
    transition: 'width 0.2s ease',
  },
  barFillLeading: {
    background: colors.accent,
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
