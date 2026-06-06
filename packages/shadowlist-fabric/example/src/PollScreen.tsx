import {
  useState,
  useRef,
  useCallback,
  useMemo,
  memo,
  type ComponentType,
} from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';
import { HeaderListItem } from './HeaderListItem';
import { useHeaderActions } from './HeaderActions';
import { Bell, CircleSlash, Globe, HalfCircle, Swatch } from './icons';
import { generateUniqueId } from './constants';
import { colors, typography, radius } from './theme';

/*
 * Poll — a deliberately SHORT (5-item) interactive list that exercises a combination of
 * features at once: a sticky header (the question), a sticky full-width footer (the live
 * tally) pinned over content that never fills the viewport, pull-to-refresh (load a fresh
 * poll) and per-row state updates. Tapping an option casts a vote; the result bars and the
 * leading highlight update live. Distinct from the other screens, which are all
 * scroll-through content / grids / drag / hierarchies.
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
// is tinted with the app accent. Tapping anywhere on the row casts a vote.
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
    return (
      <Pressable
        style={({ pressed }) => [styles.row, pressed && styles.rowPressed]}
        onPress={() => onVote(option.id)}
      >
        <View style={[styles.iconChip, leading && styles.iconChipLeading]}>
          <option.Icon
            size={22}
            color={leading ? colors.accent : colors.label}
          />
        </View>
        <View style={styles.body}>
          <View style={styles.bodyTop}>
            <Text style={styles.label}>{option.label}</Text>
            <Text style={[styles.share, leading && styles.shareLeading]}>
              {percent}%
            </Text>
          </View>
          <View style={styles.barTrack}>
            <View
              style={[
                styles.barFill,
                leading && styles.barFillLeading,
                { width: `${percent}%` },
              ]}
            />
          </View>
        </View>
      </Pressable>
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

  return (
    <View style={styles.container}>
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
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: colors.background,
    paddingHorizontal: 16,
    paddingVertical: 14,
  },
  rowPressed: {
    backgroundColor: colors.elevated,
  },
  iconChip: {
    width: 44,
    height: 44,
    borderRadius: radius.pill,
    backgroundColor: colors.elevated,
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 14,
  },
  iconChipLeading: {
    backgroundColor: colors.accentSoft,
  },
  body: {
    flex: 1,
  },
  bodyTop: {
    flexDirection: 'row',
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
    fontWeight: '600',
  },
  barTrack: {
    height: 6,
    borderRadius: radius.pill,
    backgroundColor: colors.fill,
    overflow: 'hidden',
  },
  barFill: {
    height: '100%',
    borderRadius: radius.pill,
    backgroundColor: colors.secondaryLabel,
  },
  barFillLeading: {
    backgroundColor: colors.accent,
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
