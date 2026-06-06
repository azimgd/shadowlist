import { memo, type CSSProperties } from 'react';
import { colors, typography } from './theme';

interface HeaderListItemProps {
  title?: string;
  subtitle?: string;
}

// iOS large-title header. No divider — list separators below carry the break.
export const HeaderListItem = memo(({ title = 'Header', subtitle }: HeaderListItemProps) => {
  return (
    <div style={styles.container}>
      <span style={styles.title}>{title}</span>
      {subtitle ? <span style={styles.subtitle}>{subtitle}</span> : null}
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    flexDirection: 'column',
    background: colors.background,
    padding: '4px 16px 12px',
  },
  title: {
    color: colors.label,
    ...typography.largeTitle,
  },
  subtitle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    marginTop: 2,
  },
};
