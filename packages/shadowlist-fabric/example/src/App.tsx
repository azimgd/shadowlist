import { useState } from 'react';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import TabView, { SceneMap } from 'react-native-bottom-tabs';
import { FeedScreen } from './FeedScreen';
import { ChatScreen } from './ChatScreen';

const renderScene = SceneMap({
  feed: FeedScreen,
  chat: ChatScreen,
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
  ]);

  return (
    <SafeAreaProvider>
      <TabView
        navigationState={{ index, routes }}
        renderScene={renderScene}
        onIndexChange={setIndex}
        labeled
        tabBarInactiveTintColor="#333333"
        tabBarActiveTintColor="#000000"
        tabBarStyle={{ backgroundColor: '#00000090' }}
      />
    </SafeAreaProvider>
  );
}
