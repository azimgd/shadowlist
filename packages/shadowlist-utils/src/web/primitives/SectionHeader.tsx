import { memo, type CSSProperties } from 'react';
import { colors, typography } from '../theme';

export interface SectionHeaderProps {
  title: string;
  count?: number;
}

// iOS grouped section header: uppercase label on a translucent bar.
export const SectionHeader = memo(({ title, count }: SectionHeaderProps) => {
  return (
    <div style={styles.container}>
      <span style={styles.title}>{title}</span>
      {count !== undefined ? <span style={styles.count}>{count}</span> : null}
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'flex-end',
    justifyContent: 'space-between',
    background: colors.elevated,
    padding: '10px 16px',
    boxSizing: 'border-box',
  },
  title: {
    color: colors.secondaryLabel,
    ...typography.footnote,
    fontWeight: 600,
    textTransform: 'uppercase',
  },
  count: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
  },
};
