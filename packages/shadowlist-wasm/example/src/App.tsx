import { useState, type CSSProperties } from 'react';
import { FeedScreen } from './FeedScreen';
import { ChatScreen } from './ChatScreen';
import { NestedScreen } from './NestedScreen';
import { MasonryScreen } from './MasonryScreen';
import { ContactsScreen } from './ContactsScreen';

const SCREENS = [
  { name: 'Feed', title: 'Feed', component: FeedScreen },
  { name: 'Chat', title: 'Chat', component: ChatScreen },
  { name: 'Nested', title: 'Nested', component: NestedScreen },
  { name: 'Masonry', title: 'Masonry', component: MasonryScreen },
  { name: 'Contacts', title: 'Contacts', component: ContactsScreen },
] as const;

export const App = () => {
  const [active, setActive] = useState<(typeof SCREENS)[number]['name']>('Feed');

  const ActiveScreen =
    SCREENS.find((screen) => screen.name === active)?.component ?? FeedScreen;
  const activeTitle = SCREENS.find((screen) => screen.name === active)?.title ?? 'Feed';

  const NAV_ACTIVE_COLOR = '#FF9500';
  const NAV_INACTIVE_COLOR = '#666666';

  return (
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
                  color: isActive ? NAV_ACTIVE_COLOR : NAV_INACTIVE_COLOR,
                  background: isActive ? 'rgba(255,149,0,0.12)' : 'transparent',
                }}
              >
                {screen.title}
              </button>
            );
          })}
        </nav>
      </aside>
      <main style={styles.content}>
        <header style={styles.header}>{activeTitle}</header>
        <div style={styles.screen}>
          <ActiveScreen />
        </div>
      </main>
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  root: {
    display: 'flex',
    flexDirection: 'row',
    height: '100%',
    width: '100%',
    background: '#000000',
  },
  drawer: {
    display: 'flex',
    flexDirection: 'column',
    width: 240,
    flexShrink: 0,
    background: '#000000',
    borderRight: '1px solid #1c1c1e',
    padding: 16,
    gap: 4,
  },
  brand: {
    color: '#FFFFFF',
    fontSize: 20,
    fontWeight: 700,
  },
  brandSub: {
    color: '#666666',
    fontSize: 12,
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
    fontSize: 15,
    fontWeight: 600,
    padding: '10px 12px',
    borderRadius: 8,
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
  content: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
    background: '#000000',
  },
  header: {
    height: 52,
    flexShrink: 0,
    display: 'flex',
    alignItems: 'center',
    padding: '0 16px',
    color: '#FFFFFF',
    fontSize: 17,
    fontWeight: 700,
    borderBottom: '1px solid #1c1c1e',
    background: '#000000',
  },
  screen: {
    position: 'relative',
    flex: 1,
    minHeight: 0,
    display: 'flex',
    flexDirection: 'column',
  },
};
