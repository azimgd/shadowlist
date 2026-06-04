import { memo, type CSSProperties } from 'react';

interface FooterListItemProps {
  text?: string;
}

export const FooterListItem = memo(({ text = 'End of list' }: FooterListItemProps) => {
  return (
    <div style={styles.container}>
      <span style={styles.text}>{text}</span>
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    background: '#000000',
    padding: '20px 12px',
    marginTop: 8,
    borderBottom: '1px solid #2F3336',
  },
  text: {
    color: '#71767B',
    fontSize: 14,
  },
};
