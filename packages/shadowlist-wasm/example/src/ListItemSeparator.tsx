import { memo, type CSSProperties } from 'react';

export const ListItemSeparator = memo(() => {
  return (
    <div style={styles.container}>
      <div style={styles.line} />
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  // Opaque full-width row so nothing shows through behind the inset divider.
  container: {
    background: '#000000',
  },
  line: {
    height: 1,
    background: '#2F3336',
    marginLeft: 64,
  },
};
