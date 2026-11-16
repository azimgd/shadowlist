import { useMemo } from 'react';
import { StyleSheet, Text, View } from 'react-native';
import { useQuery } from '@tanstack/react-query';
import { ShadowList } from 'shadowlist';
import { ListFooter, createStyles, useTheme } from 'shadowlist-utils/native';
import { request } from '../api/network';
import { buildStrip, type StripItem } from '../fixtures/horizontal';
import { QueryStatus } from './QueryStatus';
import { useScreenStyles } from './screenStyles';

export const HorizontalScreen = () => {
  const styles = useScreenStyles();
  const stripStyles = useStyles();
  const strip = useQuery({
    queryKey: ['horizontal'],
    queryFn: ({ signal }) => request(buildStrip, signal),
    staleTime: Infinity,
  });
  const stickyIndices = useMemo(
    () => strip.data?.flatMap((item, index) => (item.header ? [index] : [])),
    [strip.data]
  );

  if (strip.data === undefined) {
    return <QueryStatus error={strip.error} onRetry={strip.refetch} />;
  }

  return (
    <View style={styles.container}>
      <ListFooter text="Destinations by letter, scroll sideways." />
      <ShadowList
        horizontal
        data={strip.data}
        style={stripStyles.list}
        stickyIndices={stickyIndices}
        renderItem={({ item }) => <StripCell item={item} />}
      />
    </View>
  );
};

const StripCell = ({ item }: { item: StripItem }) => {
  const styles = useStyles();
  const { colors } = useTheme();
  return (
    <View style={[styles.cell, item.header && styles.header]}>
      <View
        style={[
          styles.card,
          {
            backgroundColor: item.header
              ? colors.elevated2
              : colors.avatarPalette[
                  item.colorIndex % colors.avatarPalette.length
                ],
          },
        ]}
      />
      <Text
        numberOfLines={1}
        style={[
          styles.title,
          { color: item.header ? colors.secondaryLabel : colors.onAccent },
        ]}
      >
        {item.title}
      </Text>
    </View>
  );
};

const useStyles = createStyles(({ typography, fontWeight, radius }) =>
  StyleSheet.create({
    list: { height: 140 },
    cell: {
      height: 140,
      minWidth: 96,
      maxWidth: 260,
      paddingHorizontal: 16,
      alignItems: 'center',
      justifyContent: 'center',
    },
    header: { width: 44, minWidth: 44, paddingHorizontal: 0 },
    card: {
      position: 'absolute',
      top: 8,
      bottom: 8,
      left: 4,
      right: 4,
      borderRadius: radius.md,
    },
    title: { ...typography.subhead, fontWeight: fontWeight.semibold },
  })
);
