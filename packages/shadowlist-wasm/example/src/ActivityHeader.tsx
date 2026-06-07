import { memo, type CSSProperties } from 'react';
import { colors, typography, radius } from './theme';

interface ActivityHeaderProps {
  startThreshold: number;
  endThreshold: number;
  onScrollToOffset: () => void;
  onScrollToEnd: () => void;
  onScrollToRandom: () => void;
  onCycleStartThreshold: () => void;
  onCycleEndThreshold: () => void;
}

export const ActivityHeader = memo(
  ({
    startThreshold,
    endThreshold,
    onScrollToOffset,
    onScrollToEnd,
    onScrollToRandom,
    onCycleStartThreshold,
    onCycleEndThreshold,
  }: ActivityHeaderProps) => {
    return (
      <div style={styles.container}>
        <span style={styles.title}>Activity</span>
        <span style={styles.subtitle}>Imperative scroll &amp; reach thresholds, opens at index 30</span>

        <div style={styles.row}>
          <button type="button" style={styles.action} onClick={onScrollToOffset}>
            Offset 2000
          </button>
          <button type="button" style={styles.action} onClick={onScrollToEnd}>
            Scroll to end
          </button>
          <button type="button" style={styles.action} onClick={onScrollToRandom}>
            Random
          </button>
        </div>

        <div style={styles.row}>
          <button type="button" style={styles.chip} onClick={onCycleStartThreshold}>
            <span style={styles.chipLabel}>Start threshold</span>
            <span style={styles.chipValue}>{startThreshold}</span>
          </button>
          <button type="button" style={styles.chip} onClick={onCycleEndThreshold}>
            <span style={styles.chipLabel}>End threshold</span>
            <span style={styles.chipValue}>{endThreshold}</span>
          </button>
        </div>
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
  chip: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    background: colors.elevated,
    borderRadius: radius.sm,
    border: 'none',
    padding: '8px 14px',
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
  chipLabel: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
  chipValue: {
    color: colors.accent,
    ...typography.footnote,
    fontWeight: 700,
  },
};
