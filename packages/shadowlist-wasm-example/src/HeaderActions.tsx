import {
  createContext,
  useContext,
  useEffect,
  useRef,
  type CSSProperties,
} from 'react';
import { Chevron, Viewfinder, colors } from 'shadowlist-utils/web';

export type HeaderActionHandlers = {
  onPrepend: () => void;
  onAppend: () => void;
  onScrollToRandom: () => void;
};

// App owns the top bar; screens publish their list controls here and the bar
// renders them as trailing buttons (the iOS nav-bar pattern), so nothing floats
// over the content.
export const HeaderActionsContext = createContext<
  (actions: HeaderActionHandlers | null) => void
>(() => {});

export const useHeaderActions = (handlers: HeaderActionHandlers) => {
  const setActions = useContext(HeaderActionsContext);
  const ref = useRef(handlers);
  ref.current = handlers;

  useEffect(() => {
    setActions({
      onPrepend: () => ref.current.onPrepend(),
      onAppend: () => ref.current.onAppend(),
      onScrollToRandom: () => ref.current.onScrollToRandom(),
    });
    return () => setActions(null);
  }, [setActions]);
};

const HeaderButton = ({
  onClick,
  children,
}: {
  onClick: () => void;
  children: React.ReactNode;
}) => (
  <button type="button" onClick={onClick} style={styles.button}>
    {children}
  </button>
);

export const HeaderActionButtons = ({
  actions,
}: {
  actions: HeaderActionHandlers | null;
}) => {
  if (!actions) return null;
  return (
    <div style={styles.container}>
      <HeaderButton onClick={actions.onPrepend}>
        <Chevron
          direction="up"
          color={colors.accent}
          size={22}
          strokeWidth={2.25}
        />
      </HeaderButton>
      <HeaderButton onClick={actions.onAppend}>
        <Chevron
          direction="down"
          color={colors.accent}
          size={22}
          strokeWidth={2.25}
        />
      </HeaderButton>
      <HeaderButton onClick={actions.onScrollToRandom}>
        <Viewfinder color={colors.accent} size={22} strokeWidth={2} />
      </HeaderButton>
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  button: {
    appearance: 'none',
    border: 'none',
    background: 'transparent',
    width: 34,
    height: 34,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    cursor: 'pointer',
    padding: 0,
  },
};
