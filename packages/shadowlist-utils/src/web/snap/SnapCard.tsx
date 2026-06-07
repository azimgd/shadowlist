import { memo, type CSSProperties } from 'react';
import type { SnapItem } from 'shadowlist-utils';
import { radius } from '../theme';

export interface SnapCardProps {
  element: SnapItem;
}

// Each card fills a quarter of the viewport so four are visible at rest.
export const SnapCard = memo(({ element }: SnapCardProps) => {
  return (
    <div style={styles.slot}>
      <div style={{ ...styles.card, background: element.accent }} />
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  slot: {
    height: '25dvh',
    padding: 8,
    boxSizing: 'border-box',
  },
  card: {
    width: '100%',
    height: '100%',
    borderRadius: radius.lg,
  },
};
