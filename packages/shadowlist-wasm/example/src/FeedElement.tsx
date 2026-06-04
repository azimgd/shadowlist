import { memo, useMemo, type CSSProperties } from 'react';
import { AVATAR_COLORS } from './constants';

export interface FeedElement {
  id: string;
  username: string;
  handle: string;
  text: string;
  imageUrls: string[];
  timestamp: string;
}

interface FeedElementProps {
  element: FeedElement;
  index: number;
}

export const FeedElement = memo(({ element, index }: FeedElementProps) => {
  const avatarColor = useMemo(
    () => AVATAR_COLORS[index % AVATAR_COLORS.length],
    [index]
  );

  return (
    <div style={styles.feedElement}>
      <div style={{ ...styles.avatar, backgroundColor: avatarColor }}>
        <span style={styles.avatarText}>{element.username.charAt(0)}</span>
      </div>
      <div style={styles.content}>
        <div style={styles.userInfo}>
          <span style={styles.username}>{element.username}</span>
          <span style={styles.handle}>
            {element.handle} · {index}
          </span>
        </div>
        <span style={styles.tweetText}>{element.text}</span>
        {element.imageUrls.length === 1 ? (
          <div style={styles.imageContainer}>
            <img src={element.imageUrls[0]} style={styles.image} alt="" />
          </div>
        ) : (
          <div style={styles.imageScrollView}>
            {element.imageUrls.map((imageUrl, imageIndex) => (
              <div key={imageIndex} style={styles.multiImageContainer}>
                <img src={imageUrl} style={styles.multiImage} alt="" />
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  feedElement: {
    display: 'flex',
    flexDirection: 'row',
    background: '#000000',
    borderBottom: '1px solid #2F3336',
    padding: '12px 12px',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: 20,
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
    flexShrink: 0,
  },
  avatarText: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: 700,
  },
  content: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
  },
  userInfo: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    marginBottom: 4,
  },
  username: {
    color: '#FFFFFF',
    fontSize: 15,
    fontWeight: 700,
  },
  handle: {
    color: '#71767B',
    fontSize: 15,
  },
  tweetText: {
    color: '#FFFFFF',
    fontSize: 15,
    lineHeight: '20px',
    marginBottom: 12,
    whiteSpace: 'pre-wrap',
  },
  imageContainer: {
    width: '100%',
    height: 200,
    borderRadius: 16,
    overflow: 'hidden',
    background: '#2F3336',
  },
  image: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
  imageScrollView: {
    display: 'flex',
    flexDirection: 'row',
    overflowX: 'auto',
    gap: 8,
  },
  multiImageContainer: {
    width: 280,
    height: 200,
    borderRadius: 16,
    overflow: 'hidden',
    background: '#2F3336',
    flexShrink: 0,
  },
  multiImage: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
};
