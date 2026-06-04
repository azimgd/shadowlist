import { memo, type CSSProperties } from 'react';

interface HeaderListItemProps {
  title?: string;
  subtitle?: string;
}

export const HeaderListItem = memo(({ title = 'Header', subtitle }: HeaderListItemProps) => {
  return (
    <div style={styles.container}>
      <span style={styles.title}>{title}</span>
      {subtitle && <span style={styles.subtitle}>{subtitle}</span>}
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    flexDirection: 'column',
    background: '#000000',
    padding: 12,
    marginBottom: 12,
    borderBottom: '1px solid #2F3336',
  },
  title: {
    color: '#FFFFFF',
    fontSize: 24,
    fontWeight: 700,
  },
  subtitle: {
    color: '#71767B',
    fontSize: 13,
    marginTop: 4,
  },
};
