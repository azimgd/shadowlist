import { memo, type CSSProperties } from 'react';
import { colors } from '../theme';

// Indeterminate loading spinner for infinite-scroll / pull-to-refresh footers.
export const Spinner = memo(
  ({
    size = 20,
    color = colors.secondaryLabel,
  }: {
    size?: number;
    color?: string;
  }) => {
    return (
      <div style={styles.container}>
        <style>{KEYFRAMES}</style>
        <div
          style={{
            ...styles.ring,
            width: size,
            height: size,
            border: `2px solid ${colors.fill}`,
            borderTopColor: color,
          }}
        />
      </div>
    );
  }
);

const KEYFRAMES = '@keyframes sl-spin { to { transform: rotate(360deg); } }';

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 16,
  },
  ring: {
    borderRadius: '50%',
    boxSizing: 'border-box',
    animation: 'sl-spin 0.7s linear infinite',
  },
};
