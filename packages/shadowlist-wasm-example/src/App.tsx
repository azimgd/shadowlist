import { useState, type CSSProperties } from 'react';
import { FeedScreen } from './FeedScreen';
import { ChatScreen } from './ChatScreen';
import { ActivityScreen } from './ActivityScreen';
import { NestedScreen } from './NestedScreen';
import { MasonryScreen } from './MasonryScreen';
import { ContactsScreen } from './ContactsScreen';
import { SectionListScreen } from './SectionListScreen';
import { ReorderScreen } from './ReorderScreen';
import { TreeScreen } from './TreeScreen';
import { PollScreen } from './PollScreen';
import {
  HeaderActionsContext,
  HeaderActionButtons,
  type HeaderActionHandlers,
} from './HeaderActions';
import { colors, typography, radius, FONT_FAMILY } from 'shadowlist-utils/web';

const SCREENS = [
  { name: 'Feed', title: 'Feed', component: FeedScreen },
  { name: 'Chat', title: 'Chat', component: ChatScreen },
  { name: 'Activity', title: 'Activity', component: ActivityScreen },
  { name: 'Nested', title: 'Nested', component: NestedScreen },
  { name: 'Masonry', title: 'Masonry', component: MasonryScreen },
  { name: 'Contacts', title: 'Contacts', component: ContactsScreen },
  { name: 'SectionList', title: 'Section List', component: SectionListScreen },
  { name: 'Reorder', title: 'Reorder', component: ReorderScreen },
  { name: 'Tree', title: 'Tree', component: TreeScreen },
  { name: 'Poll', title: 'Poll', component: PollScreen },
] as const;

export const App = () => {
  const [active, setActive] =
    useState<(typeof SCREENS)[number]['name']>('Feed');
  const [actions, setActions] = useState<HeaderActionHandlers | null>(null);

  const ActiveScreen =
    SCREENS.find((screen) => screen.name === active)?.component ?? FeedScreen;
  const activeTitle =
    SCREENS.find((screen) => screen.name === active)?.title ?? 'Feed';

  return (
    <HeaderActionsContext.Provider value={setActions}>
      <div style={styles.root}>
        <aside style={styles.drawer}>
          <div style={styles.brand}>Shadowlist</div>
          <div style={styles.brandSub}>WASM · React Web</div>
          <nav style={styles.nav}>
            {SCREENS.map((screen) => {
              const isActive = screen.name === active;
              return (
                <button
                  key={screen.name}
                  type="button"
                  onClick={() => setActive(screen.name)}
                  style={{
                    ...styles.navItem,
                    color: isActive ? colors.accent : colors.secondaryLabel,
                    background: isActive ? colors.accentSoft : 'transparent',
                  }}
                >
                  {screen.title}
                </button>
              );
            })}
          </nav>
        </aside>
        <main style={styles.content}>
          <header style={styles.header}>
            <span style={styles.headerTitle}>{activeTitle}</span>
            <HeaderActionButtons actions={actions} />
          </header>
          <div style={styles.screen}>
            <ActiveScreen />
          </div>
        </main>
      </div>
    </HeaderActionsContext.Provider>
  );
};

const styles: Record<string, CSSProperties> = {
  root: {
    display: 'flex',
    flexDirection: 'row',
    height: '100%',
    width: '100%',
    background: colors.background,
    fontFamily: FONT_FAMILY,
    WebkitFontSmoothing: 'antialiased',
  },
  drawer: {
    display: 'flex',
    flexDirection: 'column',
    width: 240,
    flexShrink: 0,
    background: colors.background,
    borderRight: `1px solid ${colors.separator}`,
    padding: 16,
    gap: 4,
  },
  brand: {
    color: colors.label,
    ...typography.title3,
  },
  brandSub: {
    color: colors.secondaryLabel,
    ...typography.caption,
    marginBottom: 20,
    marginTop: 2,
  },
  nav: {
    display: 'flex',
    flexDirection: 'column',
    gap: 2,
  },
  navItem: {
    appearance: 'none',
    border: 'none',
    textAlign: 'left',
    ...typography.subhead,
    fontWeight: 500,
    padding: '10px 12px',
    borderRadius: radius.sm,
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
  content: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
    background: colors.background,
  },
  header: {
    height: 52,
    flexShrink: 0,
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    padding: '0 12px 0 16px',
    background: colors.background,
  },
  headerTitle: {
    color: colors.secondaryLabel,
    ...typography.footnote,
    fontWeight: 600,
    textTransform: 'uppercase',
    letterSpacing: '0.4px',
  },
  screen: {
    position: 'relative',
    flex: 1,
    minHeight: 0,
    display: 'flex',
    flexDirection: 'column',
  },
};
