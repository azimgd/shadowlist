import { memo, type CSSProperties } from 'react';
import { colors, typography } from '../theme';

export interface ListFooterProps {
  text?: string;
}

export const ListFooter = memo(({ text = 'End of list' }: ListFooterProps) => {
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
    background: colors.background,
    padding: '20px 16px',
  },
  text: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
};
