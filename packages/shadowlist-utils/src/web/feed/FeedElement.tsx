import { memo, useMemo, type CSSProperties } from 'react';
import { AVATAR_COLORS, type FeedItem } from 'shadowlist-utils';
import { colors, typography, radius, ROW_INSET } from '../theme';

export interface FeedElementProps {
  element: FeedItem;
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
          <span style={styles.handle}>{element.handle}</span>
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
      <div style={styles.separator} />
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  feedElement: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'row',
    background: colors.background,
    padding: '12px 16px',
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
    color: colors.label,
    fontSize: 17,
    fontWeight: 600,
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
    marginBottom: 2,
  },
  username: {
    color: colors.label,
    ...typography.subhead,
    fontWeight: 600,
  },
  handle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  tweetText: {
    color: colors.label,
    ...typography.subhead,
    marginBottom: 12,
    whiteSpace: 'pre-wrap',
  },
  imageContainer: {
    width: '100%',
    height: 200,
    borderRadius: radius.lg,
    overflow: 'hidden',
    background: colors.elevated2,
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
    borderRadius: radius.lg,
    overflow: 'hidden',
    background: colors.elevated2,
    flexShrink: 0,
  },
  multiImage: {
    width: '100%',
    height: '100%',
    objectFit: 'cover',
    display: 'block',
  },
  separator: {
    position: 'absolute',
    left: ROW_INSET,
    right: 0,
    bottom: 0,
    height: 1,
    background: colors.separator,
  },
};
