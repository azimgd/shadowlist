import { memo, type CSSProperties } from 'react';

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

export const MasonryElement = memo(({ element, index }: MasonryElementProps) => {
  return (
    <div style={styles.masonryElement}>
      <div style={{ ...styles.imageContainer, height: element.height }}>
        <img src={element.imageUrl} style={styles.image} alt="" />
      </div>
      <span style={styles.title}>
        {element.title} · {index}
      </span>
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  masonryElement: {
    display: 'flex',
    flexDirection: 'column',
    background: '#000000',
    marginBottom: 12,
    padding: '0 6px',
  },
  imageContainer: {
    width: '100%',
    borderRadius: 8,
    overflow: 'hidden',
    background: '#2F3336',
    marginBottom: 8,
  },
  image: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
  title: {
    color: '#FFFFFF',
    fontSize: 14,
    lineHeight: '18px',
    padding: '0 4px',
    display: '-webkit-box',
    WebkitLineClamp: 2,
    WebkitBoxOrient: 'vertical',
    overflow: 'hidden',
  },
};
