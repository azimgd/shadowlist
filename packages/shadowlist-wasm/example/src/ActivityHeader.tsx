import { memo, type CSSProperties } from 'react';

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
        <span style={styles.subtitle}>Imperative scroll & reach thresholds</span>

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
  row: {
    display: 'flex',
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginTop: 12,
  },
  action: {
    appearance: 'none',
    border: 'none',
    background: '#FF9500',
    borderRadius: 16,
    padding: '7px 12px',
    color: '#FFFFFF',
    fontSize: 13,
    fontWeight: 600,
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
  chip: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    background: '#1C1C1E',
    borderRadius: 16,
    border: '1px solid #2F3336',
    padding: '7px 12px',
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
  chipLabel: {
    color: '#71767B',
    fontSize: 13,
  },
  chipValue: {
    color: '#FF9500',
    fontSize: 13,
    fontWeight: 700,
  },
};
