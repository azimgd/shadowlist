import { memo, useMemo, type CSSProperties } from 'react';
import { AVATAR_COLORS } from './constants';

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
        <div
          style={{
            ...styles.bubble,
            ...(isFromMe ? styles.bubbleFromMe : styles.bubbleFromThem),
          }}
        >
          <span style={styles.username}>
            {username} · {index}
          </span>
          <span style={styles.text}>{text}</span>
          <span style={styles.timestamp}>
            {new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
          </span>
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
    width: 32,
    height: 32,
    borderRadius: 16,
    marginRight: 8,
    marginBottom: 2,
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    flexShrink: 0,
  },
  avatarText: {
    color: '#FFFFFF',
    fontSize: 12,
    fontWeight: 600,
  },
  bubble: {
    display: 'flex',
    flexDirection: 'column',
    maxWidth: '75%',
    padding: '10px 16px',
    borderRadius: 20,
    margin: '2px 0',
  },
  bubbleFromMe: {
    background: '#0A84FF',
    borderBottomRightRadius: 4,
  },
  bubbleFromThem: {
    background: '#2C2C2E',
    borderBottomLeftRadius: 4,
  },
  username: {
    color: '#FFFFFF',
    fontSize: 14,
    fontWeight: 600,
    marginBottom: 4,
    opacity: 0.8,
  },
  text: {
    color: '#FFFFFF',
    fontSize: 16,
    lineHeight: '20px',
    whiteSpace: 'pre-wrap',
  },
  timestamp: {
    color: '#FFFFFF',
    fontSize: 12,
    marginTop: 4,
    opacity: 0.5,
  },
  singleImageContainer: {
    width: 240,
    height: 320,
    borderRadius: 16,
    overflow: 'hidden',
    background: '#2F3336',
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
    borderRadius: 8,
    overflow: 'hidden',
    background: '#2F3336',
  },
  imageGridImage: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
};
