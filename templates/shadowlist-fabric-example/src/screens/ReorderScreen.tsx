import { useCallback } from 'react';
import { View } from 'react-native';
import { Reorder, ListFooter, type ContactItem } from 'shadowlist-utils/native';
import { haptics } from '../haptics';
import { useScreenStyles } from './screenStyles';
import { QueryStatus } from './QueryStatus';
import { HOLD_TO_DRAG } from './holdToDrag';
import { useFavoritesQuery, useReorderFavorites } from '../queries/contacts';

export const ReorderScreen = () => {
  const styles = useScreenStyles();
  const favorites = useFavoritesQuery();
  const { mutate: saveOrder } = useReorderFavorites();

  const handleMoveItem = useCallback(
    ({ data: reordered }: { data: ContactItem[] }) => {
      haptics.drop();
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
        onMoveItem={handleMoveItem}
        ListHeaderComponent={
          <ListFooter
            text={`${HOLD_TO_DRAG} a traveller, then drag to change the order.`}
          />
        }
      />
    </View>
  );
};
