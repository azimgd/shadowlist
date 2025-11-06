import { useState } from 'react';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import TabView, { SceneMap } from 'react-native-bottom-tabs';
import { FeedScreen } from './FeedScreen';
import { ChatScreen } from './ChatScreen';
import { GridScreen } from './GridScreen';

const renderScene = SceneMap({
  feed: FeedScreen,
  chat: ChatScreen,
  grid: GridScreen,
});

export default function App() {
  const [index, setIndex] = useState(0);
  const [routes] = useState([
    {
      key: 'feed',
      title: 'Feed',
      focusedIcon: { sfSymbol: 'newspaper.fill' },
    },
    {
      key: 'chat',
      title: 'Chat',
      focusedIcon: { sfSymbol: 'message.fill' },
    },
    {
      key: 'grid',
      title: 'Grid',
      focusedIcon: { sfSymbol: 'square.grid.2x2.fill' },
    },
  ]);

  return (
    <SafeAreaProvider>
      <TabView
        navigationState={{ index, routes }}
        renderScene={renderScene}
        onIndexChange={setIndex}
        labeled
        tabBarInactiveTintColor="#333333"
        tabBarActiveTintColor="#FF9500"
        tabBarStyle={{ backgroundColor: '#00000090' }}
      />
    </SafeAreaProvider>
  );
}
