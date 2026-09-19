import { View } from 'react-native';
import { Snap } from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { generateSnapElement } from './fixtures/snap';

const data = Array.from({ length: 50 }, (_, index) =>
  generateSnapElement(index)
);

export const SnapScreen = () => {
  const styles = useScreenStyles();
  return (
    <View style={styles.container}>
      <Snap.List data={data} style={styles.list} />
    </View>
  );
};
