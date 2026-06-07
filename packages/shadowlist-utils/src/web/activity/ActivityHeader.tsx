import { memo, type CSSProperties } from 'react';
import { colors, typography, radius } from '../theme';

export interface ActivityHeaderAction {
  label: string;
  onPress: () => void;
}

export interface ActivityHeaderProps {
  title?: string;
  subtitle?: string;
  actions?: ActivityHeaderAction[];
}

// Large-title header with an optional row of tinted action buttons.
export const ActivityHeader = memo(
  ({ title = 'Activity', subtitle, actions }: ActivityHeaderProps) => {
    return (
      <div style={styles.container}>
        <span style={styles.title}>{title}</span>
        {subtitle ? <span style={styles.subtitle}>{subtitle}</span> : null}
        {actions && actions.length > 0 ? (
          <div style={styles.row}>
            {actions.map((action) => (
              <button
                key={action.label}
                type="button"
                style={styles.action}
                onClick={action.onPress}
              >
                {action.label}
              </button>
            ))}
          </div>
        ) : null}
      </div>
    );
  }
);

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
  row: {
    display: 'flex',
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginTop: 14,
  },
  action: {
    appearance: 'none',
    border: 'none',
    background: colors.accentSoft,
    borderRadius: radius.sm,
    padding: '8px 14px',
    color: colors.accent,
    ...typography.footnote,
    fontWeight: 600,
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
};
