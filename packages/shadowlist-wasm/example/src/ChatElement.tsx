import { memo, useMemo, type CSSProperties } from 'react';
import { AVATAR_COLORS } from './constants';
import { colors, typography, radius } from './theme';

interface ChatElementProps {
  id: string;
  index: number;
  text: string;
  isFromMe: boolean;
  imageUrl?: string;
  imageUrls?: string[];
}

export const ChatElement = memo(
  ({ index, text, isFromMe, imageUrl, imageUrls }: ChatElementProps) => {
    const avatarColor = useMemo(
      () => AVATAR_COLORS[index % AVATAR_COLORS.length],
      [index]
    );

    const initials = useMemo(() => {
      const firstLetter = String.fromCharCode(65 + (index % 26));
      const secondLetter = String.fromCharCode(65 + ((index * 3) % 26));
      return `${firstLetter}${secondLetter}`;
    }, [index]);

    const username = useMemo(() => {
      const names = ['Alice', 'Bob', 'Charlie', 'Diana', 'Eve', 'Frank', 'Grace', 'Henry'];
      return names[index % names.length];
    }, [index]);

    const hasImageGrid = imageUrls && imageUrls.length > 0;
    const hasSingleImage = imageUrl && !text;

    const containerStyle: CSSProperties = {
      ...styles.container,
      justifyContent: isFromMe ? 'flex-end' : 'flex-start',
    };

    const avatar = !isFromMe && (
      <div style={{ ...styles.avatar, backgroundColor: avatarColor }}>
        <span style={styles.avatarText}>{initials}</span>
      </div>
    );

    if (hasImageGrid) {
      return (
        <div style={containerStyle}>
          {avatar}
          <div style={styles.imageGridContainer}>
            <div style={styles.imageGridRow}>
              <div style={styles.imageGridElement}>
                <img src={imageUrls[0]} style={styles.imageGridImage} alt="" />
              </div>
              <div style={styles.imageGridElement}>
                <img src={imageUrls[1]} style={styles.imageGridImage} alt="" />
              </div>
            </div>
            <div style={styles.imageGridRow}>
              <div style={styles.imageGridElement}>
                <img src={imageUrls[2]} style={styles.imageGridImage} alt="" />
              </div>
              <div style={styles.imageGridElement}>
                <img src={imageUrls[3]} style={styles.imageGridImage} alt="" />
              </div>
            </div>
          </div>
        </div>
      );
    }

    if (hasSingleImage) {
      return (
        <div style={containerStyle}>
          {avatar}
          <div style={styles.singleImageContainer}>
            <img src={imageUrl} style={styles.singleImage} alt="" />
          </div>
        </div>
      );
    }

    return (
      <div style={containerStyle}>
        {avatar}
        <div style={styles.bubbleColumn}>
          {!isFromMe ? <span style={styles.sender}>{username}</span> : null}
          <div
            style={{
              ...styles.bubble,
              ...(isFromMe ? styles.bubbleFromMe : styles.bubbleFromThem),
            }}
          >
            <span style={styles.text}>{text}</span>
          </div>
        </div>
      </div>
    );
  }
);

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'flex-end',
    padding: '2px 12px',
  },
  avatar: {
    width: 30,
    height: 30,
    borderRadius: 15,
    marginRight: 8,
    marginBottom: 2,
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    flexShrink: 0,
  },
  avatarText: {
    color: colors.label,
    fontSize: 12,
    fontWeight: 600,
  },
  bubbleColumn: {
    display: 'flex',
    flexDirection: 'column',
    maxWidth: '75%',
  },
  sender: {
    color: colors.secondaryLabel,
    ...typography.caption,
    marginLeft: 12,
    marginBottom: 2,
  },
  bubble: {
    display: 'flex',
    flexDirection: 'column',
    padding: '8px 14px',
    borderRadius: radius.lg + 2,
  },
  bubbleFromMe: {
    background: colors.blue,
    borderBottomRightRadius: 5,
    alignSelf: 'flex-end',
  },
  bubbleFromThem: {
    background: colors.elevated2,
    borderBottomLeftRadius: 5,
    alignSelf: 'flex-start',
  },
  text: {
    color: colors.label,
    ...typography.body,
    whiteSpace: 'pre-wrap',
  },
  singleImageContainer: {
    width: 240,
    height: 320,
    borderRadius: radius.lg,
    overflow: 'hidden',
    background: colors.elevated2,
    margin: '2px 0',
  },
  singleImage: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
  imageGridContainer: {
    display: 'flex',
    flexDirection: 'column',
    width: 240,
    margin: '2px 0',
  },
  imageGridRow: {
    display: 'flex',
    flexDirection: 'row',
    gap: 2,
    marginBottom: 2,
  },
  imageGridElement: {
    width: 119,
    height: 119,
    borderRadius: radius.sm,
    overflow: 'hidden',
    background: colors.elevated2,
  },
  imageGridImage: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
};
