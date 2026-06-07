import { memo, type CSSProperties } from 'react';
import type { NestedCard as NestedCardData } from 'shadowlist-utils';
import { colors, typography, radius } from '../theme';

export interface NestedCardProps {
  element: NestedCardData;
}

export const NestedCard = memo(({ element }: NestedCardProps) => {
  return (
    <div style={styles.nestedElement}>
      <div style={styles.imageContainer}>
        <img src={element.imageUrl} style={styles.image} alt="" />
      </div>
      <span style={styles.title}>{element.title}</span>
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  nestedElement: {
    display: 'flex',
    flexDirection: 'column',
    width: 180,
    marginLeft: 16,
  },
  imageContainer: {
    width: 180,
    height: 220,
    borderRadius: radius.md,
    overflow: 'hidden',
    background: colors.elevated2,
    marginBottom: 8,
  },
  image: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
  title: {
    color: colors.label,
    ...typography.subhead,
    display: '-webkit-box',
    WebkitLineClamp: 2,
    WebkitBoxOrient: 'vertical',
    overflow: 'hidden',
  },
};
