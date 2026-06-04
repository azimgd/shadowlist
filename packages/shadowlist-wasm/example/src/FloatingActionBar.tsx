import { useState, type CSSProperties } from 'react';

interface FloatingActionBarProps {
  onPrepend: () => void;
  onAppend: () => void;
  onScrollToIndex: (index: number) => void;
  dataLength: number;
}

export const FloatingActionBar = ({
  onPrepend,
  onAppend,
  onScrollToIndex,
  dataLength,
}: FloatingActionBarProps) => {
  const [targetIndex, setTargetIndex] = useState<number | null>(null);

  const handleScrollToRandom = () => {
    const randomIndex = Math.floor(Math.random() * dataLength);
    setTargetIndex(randomIndex);
    onScrollToIndex(randomIndex);
  };

  return (
    <div style={styles.container}>
      <button type="button" style={styles.button} onClick={onPrepend}>
        ↑
      </button>
      <button type="button" style={styles.button} onClick={onAppend}>
        ↓
      </button>
      <button type="button" style={styles.scrollButton} onClick={handleScrollToRandom}>
        {targetIndex ?? '🎯'}
      </button>
    </div>
  );
};

const buttonBase: CSSProperties = {
  appearance: 'none',
  border: 'none',
  height: 32,
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  color: '#ffffff',
  fontSize: 14,
  fontWeight: 500,
  cursor: 'pointer',
  fontFamily: 'inherit',
};

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'absolute',
    top: 16,
    right: 16,
    zIndex: 10,
    display: 'flex',
    flexDirection: 'row',
    gap: 8,
    background: 'rgba(28, 28, 30, 0.8)',
    borderRadius: 18,
    padding: 4,
  },
  button: {
    ...buttonBase,
    width: 32,
    borderRadius: 16,
    background: '#FF9500',
  },
  scrollButton: {
    ...buttonBase,
    minWidth: 32,
    borderRadius: 16,
    background: '#34C759',
    padding: '0 8px',
  },
};
