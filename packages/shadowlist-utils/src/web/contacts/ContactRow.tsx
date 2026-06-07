import {
  memo,
  useMemo,
  useRef,
  useState,
  type CSSProperties,
  type PointerEvent,
} from 'react';
import { AVATAR_COLORS, type ContactItem } from 'shadowlist-utils';
import { colors, typography, ROW_INSET } from '../theme';
import { Chevron } from '../icons';

export interface ContactRowProps {
  element: ContactItem;
  index: number;
  // When provided, the swipe-revealed delete button calls this with the id.
  onDelete?: (id: string) => void;
}

const SWIPE_THRESHOLD = -80;
const DELETE_BUTTON_WIDTH = 80;

export const ContactRow = memo(({ element, index, onDelete }: ContactRowProps) => {
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
    dragRef.current = { active: true, startX: event.clientX, startTranslate: translateX };
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
      <button
        type="button"
        style={{
          ...styles.deleteButton,
          width: Math.abs(translateX),
          opacity: translateX < -5 ? 1 : 0,
        }}
        onClick={() => onDelete?.(element.id)}
      >
        <span style={styles.deleteButtonText}>Delete</span>
      </button>
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
              {element.firstName} {element.lastName}
            </span>
            <span style={styles.phoneNumber}>{element.phoneNumber}</span>
          </div>
          <Chevron direction="right" color={colors.tertiaryLabel} size={20} strokeWidth={2} />
          <div style={styles.separator} />
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
    position: 'relative',
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    background: colors.background,
    padding: '12px 12px 12px 16px',
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
  name: {
    color: colors.label,
    ...typography.body,
    marginBottom: 1,
  },
  phoneNumber: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  separator: {
    position: 'absolute',
    left: ROW_INSET,
    right: 0,
    bottom: 0,
    height: 1,
    background: colors.separator,
  },
  deleteButton: {
    position: 'absolute',
    right: 0,
    top: 0,
    bottom: 0,
    minWidth: DELETE_BUTTON_WIDTH,
    background: colors.red,
    border: 'none',
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    cursor: 'pointer',
  },
  deleteButtonText: {
    color: colors.label,
    ...typography.subhead,
    fontWeight: 600,
  },
};
