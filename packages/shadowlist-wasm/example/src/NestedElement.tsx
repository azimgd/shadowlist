import { memo, type CSSProperties } from 'react';
import { Shadowlist } from 'shadowlist-wasm';

export interface NestedElementChild {
  id: string;
  imageUrl: string;
  title: string;
}

export interface NestedElement {
  id: string;
  title: string;
  elements: NestedElementChild[];
}

interface NestedElementProps {
  element: NestedElement;
  index: number;
}

const NestedElementChildComponent = memo(
  ({ element }: { element: NestedElementChild }) => {
    return (
      <div style={styles.nestedElement}>
        <div style={styles.imageContainer}>
          <img src={element.imageUrl} style={styles.image} alt="" />
        </div>
        <span style={styles.title}>{element.title}</span>
      </div>
    );
  }
);

export const NestedElement = memo(({ element, index }: NestedElementProps) => {
  return (
    <div style={styles.container}>
      <span style={styles.sectionTitle}>
        {element.title} · {index}
      </span>
      <Shadowlist
        data={element.elements}
        horizontal
        style={styles.horizontalList}
        renderElement={({ element: nestedElementChild }) => (
          <NestedElementChildComponent element={nestedElementChild} />
        )}
      />
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    flexDirection: 'column',
    marginBottom: 12,
    height: 300,
  },
  sectionTitle: {
    color: '#FFFFFF',
    fontSize: 20,
    fontWeight: 700,
    padding: '0 12px',
    marginBottom: 12,
  },
  horizontalList: {
    flex: 1,
    background: '#000000',
  },
  nestedElement: {
    display: 'flex',
    flexDirection: 'column',
    width: 180,
    marginLeft: 12,
  },
  imageContainer: {
    width: 180,
    height: 220,
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
    display: '-webkit-box',
    WebkitLineClamp: 2,
    WebkitBoxOrient: 'vertical',
    overflow: 'hidden',
  },
};
