import { memo, useMemo, useRef, useState, type CSSProperties, type PointerEvent } from 'react';
import { AVATAR_COLORS } from './constants';

export interface ContactElement {
  id: string;
  firstName: string;
  lastName: string;
  phoneNumber: string;
}

interface ContactElementProps {
  element: ContactElement;
  index: number;
}

const SWIPE_THRESHOLD = -80;
const DELETE_BUTTON_WIDTH = 80;

export const ContactElement = memo(({ element, index }: ContactElementProps) => {
  const avatarColor = useMemo(
    () => AVATAR_COLORS[index % AVATAR_COLORS.length],
    [index]
  );

  const initials = useMemo(
    () => `${element.firstName.charAt(0)}${element.lastName.charAt(0)}`,
    [element.firstName, element.lastName]
  );

  const [translateX, setTranslateX] = useState(0);
  const dragRef = useRef({ active: false, startX: 0, startTranslate: 0 });

  const onPointerDown = (event: PointerEvent<HTMLDivElement>) => {
    dragRef.current = {
      active: true,
      startX: event.clientX,
      startTranslate: translateX,
    };
    event.currentTarget.setPointerCapture(event.pointerId);
  };

  const onPointerMove = (event: PointerEvent<HTMLDivElement>) => {
    if (!dragRef.current.active) return;
    const next = dragRef.current.startTranslate + (event.clientX - dragRef.current.startX);
    setTranslateX(Math.min(0, next));
  };

  const onPointerUp = () => {
    if (!dragRef.current.active) return;
    dragRef.current.active = false;
    setTranslateX((current) => (current < SWIPE_THRESHOLD ? -DELETE_BUTTON_WIDTH : 0));
  };

  return (
    <div style={styles.container}>
      <div
        style={{
          ...styles.deleteButton,
          width: Math.abs(translateX),
          opacity: translateX < -5 ? 1 : 0,
        }}
      >
        <span style={styles.deleteButtonText}>Delete</span>
      </div>
      <div
        style={{
          ...styles.wrapper,
          transform: `translateX(${translateX}px)`,
          transition: dragRef.current.active ? 'none' : 'transform 0.2s ease',
        }}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerCancel={onPointerUp}
      >
        <div style={styles.contactElement}>
          <div style={{ ...styles.avatar, backgroundColor: avatarColor }}>
            <span style={styles.avatarText}>{initials}</span>
          </div>
          <div style={styles.content}>
            <span style={styles.name}>
              {element.firstName} {element.lastName} · {index}
            </span>
            <span style={styles.phoneNumber}>{element.phoneNumber}</span>
          </div>
          <span style={styles.chevron}>›</span>
        </div>
      </div>
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'relative',
    overflow: 'hidden',
    display: 'flex',
  },
  wrapper: {
    position: 'relative',
    flex: 1,
    touchAction: 'pan-y',
  },
  contactElement: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
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
    fontWeight: 600,
  },
  content: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
  },
  name: {
    color: '#FFFFFF',
    fontSize: 17,
    fontWeight: 400,
    marginBottom: 2,
  },
  phoneNumber: {
    color: '#71767B',
    fontSize: 15,
  },
  chevron: {
    color: '#71767B',
    fontSize: 24,
    fontWeight: 300,
  },
  deleteButton: {
    position: 'absolute',
    right: 0,
    top: 0,
    bottom: 0,
    minWidth: DELETE_BUTTON_WIDTH,
    background: '#FF3B30',
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
  },
  deleteButtonText: {
    color: '#FFFFFF',
    fontSize: 15,
    fontWeight: 600,
  },
};
