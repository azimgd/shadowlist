import { useCallback } from 'react';
import { View } from 'react-native';
import { Reorder, ListFooter, type ContactItem } from 'shadowlist-utils/native';
import { useScreenStyles } from '@example/screens/screenStyles';
import { QueryStatus } from '@example/screens/QueryStatus';
import {
  useFavoritesQuery,
  useReorderFavorites,
} from '@example/queries/contacts';

export const ReorderScreen = () => {
  const styles = useScreenStyles();
  const favorites = useFavoritesQuery();
  const { mutate: saveOrder } = useReorderFavorites();

  const handleReorder = useCallback(
    ({ data: reordered }: { data: ContactItem[] }) => {
      saveOrder(reordered);
    },
    [saveOrder]
  );

  if (favorites.data === undefined) {
    return <QueryStatus error={favorites.error} onRetry={favorites.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Reorder.List
        data={favorites.data}
        style={styles.list}
        onReorder={handleReorder}
        ListHeaderComponent={
          <ListFooter text="Click and hold a traveller, then drag to change the order." />
        }
      />
    </View>
  );
};
