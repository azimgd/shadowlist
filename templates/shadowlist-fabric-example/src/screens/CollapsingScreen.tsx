import { useMemo } from 'react';
import { StyleSheet, View } from 'react-native';
import { ShadowList } from 'shadowlist';
import {
  ListFooter,
  Reorder,
  createStyles,
  type ContactItem,
} from 'shadowlist-utils/native';
import { useTravellersQuery } from '../queries/travellers';
import { QueryStatus } from './QueryStatus';
import { useScreenStyles } from './screenStyles';

type CollapsingItem = { id: string; contact?: ContactItem };

export const CollapsingScreen = () => {
  const styles = useScreenStyles();
  const barStyles = useStyles();
  const travellers = useTravellersQuery('Collapsing');
  const data = useMemo<CollapsingItem[]>(
    () => [
      { id: 'bar' },
      ...(travellers.data ?? []).map((contact) => ({
        id: contact.id,
        contact,
      })),
    ],
    [travellers.data]
  );

  if (travellers.data === undefined) {
    return (
      <QueryStatus error={travellers.error} onRetry={travellers.refetch} />
    );
  }

  return (
    <View style={styles.container}>
      <ShadowList
        data={data}
        style={styles.list}
        ListHeaderComponent={<View style={barStyles.space} />}
        stickyIndices={[0]}
        renderItem={({ item }) =>
          item.contact ? (
            <Reorder.Row item={item.contact} />
          ) : (
            <ListFooter text="A bar that collapses as the list scrolls." />
          )
        }
      />
    </View>
  );
};

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    // With the 58 point caption this makes the 200 point bar of the native screen.
    space: { height: 142, backgroundColor: colors.elevated },
  })
);
