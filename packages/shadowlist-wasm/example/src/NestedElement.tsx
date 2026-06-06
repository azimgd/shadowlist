import { memo, type CSSProperties } from 'react';
import { Shadowlist } from 'shadowlist-wasm';
import { colors, typography, radius } from './theme';

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

export const NestedElement = memo(({ element }: NestedElementProps) => {
  return (
    <div style={styles.container}>
      <span style={styles.sectionTitle}>{element.title}</span>
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
    marginBottom: 16,
    height: 300,
  },
  sectionTitle: {
    color: colors.label,
    ...typography.title3,
    padding: '0 16px',
    marginBottom: 12,
  },
  horizontalList: {
    flex: 1,
    background: colors.background,
  },
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
