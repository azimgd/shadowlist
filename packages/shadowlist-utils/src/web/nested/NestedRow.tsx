import { memo, type CSSProperties } from 'react';
import { Shadowlist, type ShadowlistProps } from 'shadowlist-wasm';
import type { NestedItem, NestedCard as NestedCardData } from 'shadowlist-utils';
import { NestedCard } from './NestedCard';
import { colors, typography } from '../theme';

export interface NestedRowProps {
  element: NestedItem;
}

const renderNestedCard: ShadowlistProps<NestedCardData>['renderElement'] = ({
  element,
}) => <NestedCard element={element} />;

// One titled row containing a horizontal, virtualized carousel of cards.
export const NestedRow = memo(({ element }: NestedRowProps) => {
  return (
    <div style={styles.container}>
      <span style={styles.sectionTitle}>{element.title}</span>
      <Shadowlist
        data={element.elements}
        horizontal
        style={styles.horizontalList}
        renderElement={renderNestedCard}
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
};
