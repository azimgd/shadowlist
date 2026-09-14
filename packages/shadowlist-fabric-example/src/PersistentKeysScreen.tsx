import { useCallback, useEffect, useState } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { ShadowList } from 'shadowlist';
import { ListHeader, colors, typography } from 'shadowlist-utils/native';

interface Row {
  id: string;
  index: number;
  pinned: boolean;
}

/*
 * Two rows are pinned by key: the first row and one deep in the list. Everything else
 * virtualizes normally.
 */
const PINNED_INDICES = [0, 40];

const DATA: Row[] = Array.from({ length: 200 }, (_, index) => ({
  id: `row-${index}`,
  index,
  pinned: PINNED_INDICES.includes(index),
}));

/*
 * persistentKeys takes keys (keyExtractor output), not indices, so a pinned row keeps
 * its identity even if the data is reordered or items are inserted above it.
 */
const PERSISTENT_KEYS = DATA.filter((row) => row.pinned).map((row) => row.id);

/*
 * Each row counts the seconds since it last mounted. A normal (virtualized) row resets
 * to 0s every time you scroll it off-screen and back, because it unmounts and remounts.
 * A persistent row keeps counting: it is never unmounted, so its timer never
 * resets, the visible proof that persistentKeys holds the row in the tree.
 */
const RowView = ({ row }: { row: Row }) => {
  const [secondsAlive, setSecondsAlive] = useState(0);

  useEffect(() => {
    const id = setInterval(
      () => setSecondsAlive((seconds) => seconds + 1),
      1000
    );
    return () => clearInterval(id);
  }, []);

  return (
    <View style={[styles.row, row.pinned && styles.rowPinned]}>
      <Text style={[styles.rowLabel, row.pinned && styles.rowLabelPinned]}>
        {row.pinned ? `★ Pinned row ${row.index}` : `Item ${row.index}`}
      </Text>
      <Text style={[styles.rowMeta, row.pinned && styles.rowMetaPinned]}>
        {row.pinned
          ? `alive ${secondsAlive}s · never unmounts`
          : `alive ${secondsAlive}s`}
      </Text>
    </View>
  );
};

export const PersistentKeysScreen = () => {
  const renderElement = useCallback(
    ({ element }: { element: Row }) => <RowView row={element} />,
    []
  );

  return (
    <View style={styles.container}>
      <ShadowList
        data={DATA}
        style={styles.list}
        persistentKeys={PERSISTENT_KEYS}
        renderElement={renderElement}
        ListHeaderComponent={
          <ListHeader
            title="Persistent Keys"
            subtitle="Rows 0 and 40 stay mounted. Scroll away and back — normal rows reset their timer, pinned rows keep counting."
          />
        }
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
    paddingHorizontal: 16,
    paddingVertical: 18,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: colors.separator,
    backgroundColor: colors.background,
    gap: 4,
  },
  rowPinned: {
    backgroundColor: colors.accentSoft,
  },
  rowLabel: {
    color: colors.label,
    ...typography.body,
  },
  rowLabelPinned: {
    color: colors.accent,
    ...typography.headline,
  },
  rowMeta: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
  rowMetaPinned: {
    color: colors.accent,
    ...typography.footnote,
  },
});
