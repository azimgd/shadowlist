import { useRef } from 'react';
import { View } from 'react-native';
import { ShadowList } from 'shadowlist';
import { Reorder } from 'shadowlist-utils/native';
import { useChangeTravellers, useTravellersQuery } from '../queries/travellers';
import { useHeaderMenu } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { useScreenStyles } from './screenStyles';

export const ChangesScreen = () => {
  const styles = useScreenStyles();
  const travellers = useTravellersQuery('Changes');
  const { mutate: change } = useChangeTravellers('Changes');
  const fresh = useRef(1000);

  useHeaderMenu([
    [
      {
        label: 'Change Travellers',
        symbol: 'arrow.triangle.2.circlepath',
        onPress: () => {
          change(fresh.current);
          fresh.current += 2;
        },
      },
    ],
  ]);

  if (travellers.data === undefined) {
    return (
      <QueryStatus error={travellers.error} onRetry={travellers.refetch} />
    );
  }

  return (
    <View style={styles.container}>
      <ShadowList
        data={travellers.data}
        style={styles.list}
        renderItem={({ item }) => <Reorder.Row item={item} />}
      />
    </View>
  );
};
