import { useCallback } from 'react';
import { View } from 'react-native';
import { Reorder, ListHeader, type ContactItem } from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { QueryStatus } from './QueryStatus';
import { useFavoritesQuery, useReorderFavorites } from './queries/contacts';

export const ReorderScreen = () => {
  const styles = useScreenStyles();
  const favorites = useFavoritesQuery();
  const { mutate: saveOrder } = useReorderFavorites();

  const handleReorder = useCallback(
    ({ data: reordered }: { data: ContactItem[] }) => saveOrder(reordered),
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
          <ListHeader
            title="Boarding Order"
            subtitle="Hold a traveller, then drag to reorder"
          />
        }
      />
    </View>
  );
};
