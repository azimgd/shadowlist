import { type CSSProperties } from 'react';
import { Snap, colors } from 'shadowlist-utils/web';
import { generateSnapElement } from 'shadowlist-utils';

const data = Array.from({ length: 50 }, (_, index) =>
  generateSnapElement(index)
);

export const SnapScreen = () => {
  return (
    <div style={styles.container}>
      <Snap.List
        data={data}
        style={styles.list}
        keyExtractor={(item) => item.id}
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
};
