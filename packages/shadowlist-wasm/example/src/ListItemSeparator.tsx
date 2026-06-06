import { memo, type CSSProperties } from 'react';
import { colors, ROW_INSET } from './theme';

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
    background: colors.background,
  },
  line: {
    height: 1,
    background: colors.separator,
    marginLeft: ROW_INSET,
  },
};
