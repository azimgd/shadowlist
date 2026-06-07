import { memo, type CSSProperties } from 'react';
import { colors, typography, radius } from '../theme';
import type { PollOption as PollOptionData } from './data';

export interface PollOptionProps {
  option: PollOptionData;
  total: number;
  leading?: boolean;
  onVote?: (id: string) => void;
}

// One option: icon chip, label, its share, and an inline result bar. The leader
// is tinted with the app accent; clicking the row casts a vote.
export const PollOptionRow = memo(
  ({ option, total, leading = false, onVote }: PollOptionProps) => {
    const share = total > 0 ? option.votes / total : 0;
    const percent = Math.round(share * 100);
    const Icon = option.Icon;
    return (
      <div style={styles.row} onClick={() => onVote?.(option.id)}>
        <div style={{ ...styles.iconChip, ...(leading ? styles.iconChipLeading : null) }}>
          <Icon size={22} color={leading ? colors.accent : colors.label} />
        </div>
        <div style={styles.body}>
          <div style={styles.bodyTop}>
            <span style={styles.label}>{option.label}</span>
            <span style={{ ...styles.share, ...(leading ? styles.shareLeading : null) }}>
              {percent}%
            </span>
          </div>
          <div style={styles.barTrack}>
            <div
              style={{
                ...styles.barFill,
                ...(leading ? styles.barFillLeading : null),
                width: `${percent}%`,
              }}
            />
          </div>
        </div>
      </div>
    );
  }
);

const styles: Record<string, CSSProperties> = {
  row: {
    display: 'flex',
    alignItems: 'center',
    padding: '14px 16px',
    background: colors.background,
    boxSizing: 'border-box',
    cursor: 'pointer',
  },
  iconChip: {
    width: 44,
    height: 44,
    borderRadius: radius.pill,
    background: colors.elevated,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 14,
    flexShrink: 0,
  },
  iconChipLeading: {
    background: colors.accentSoft,
  },
  body: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
  },
  bodyTop: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  label: {
    color: colors.label,
    ...typography.headline,
  },
  share: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  shareLeading: {
    color: colors.accent,
    fontWeight: 600,
  },
  barTrack: {
    height: 6,
    borderRadius: radius.pill,
    background: colors.fill,
    overflow: 'hidden',
  },
  barFill: {
    height: '100%',
    borderRadius: radius.pill,
    background: colors.secondaryLabel,
    transition: 'width 0.2s ease',
  },
  barFillLeading: {
    background: colors.accent,
  },
};
