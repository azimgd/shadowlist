import { memo, type CSSProperties } from 'react';
import { colors, typography, radius } from './theme';

export interface MasonryElement {
  id: string;
  imageUrl: string;
  title: string;
  height: number;
}

interface MasonryElementProps {
  element: MasonryElement;
  index: number;
}

export const MasonryElement = memo(({ element }: MasonryElementProps) => {
  return (
    <div style={styles.masonryElement}>
      <div style={{ ...styles.imageContainer, height: element.height }}>
        <img src={element.imageUrl} style={styles.image} alt="" />
      </div>
      <span style={styles.title}>{element.title}</span>
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  masonryElement: {
    display: 'flex',
    flexDirection: 'column',
    background: colors.background,
    marginBottom: 12,
    padding: '0 6px',
  },
  imageContainer: {
    width: '100%',
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
    padding: '0 4px',
    display: '-webkit-box',
    WebkitLineClamp: 2,
    WebkitBoxOrient: 'vertical',
    overflow: 'hidden',
  },
};
