import 'react-native-gesture-handler';
import { enableScreens } from 'react-native-screens';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { NavigationContainer } from '@react-navigation/native';
import { createDrawerNavigator } from '@react-navigation/drawer';
import { FeedScreen } from './FeedScreen';
import { ChatScreen } from './ChatScreen';
import { ActivityScreen } from './ActivityScreen';
import { NestedScreen } from './NestedScreen';
import { MasonryScreen } from './MasonryScreen';
import { ContactsScreen } from './ContactsScreen';
import { SectionListScreen } from './SectionListScreen';
import { ReorderScreen } from './ReorderScreen';
import { TreeScreen } from './TreeScreen';

enableScreens();

const Drawer = createDrawerNavigator();

export default function App() {
  return (
    <SafeAreaProvider>
      <NavigationContainer>
        <Drawer.Navigator
          initialRouteName="Feed"
          screenOptions={{
            drawerStyle: {
              backgroundColor: '#000000',
            },
            drawerActiveTintColor: '#FF9500',
            drawerInactiveTintColor: '#666666',
            headerStyle: {
              backgroundColor: '#000000',
            },
            headerTintColor: '#FFFFFF',
          }}
        >
          <Drawer.Screen
            name="Feed"
            component={FeedScreen}
            options={{
              title: 'Feed',
            }}
          />
          <Drawer.Screen
            name="Chat"
            component={ChatScreen}
            options={{
              title: 'Chat',
            }}
          />
          <Drawer.Screen
            name="Activity"
            component={ActivityScreen}
            options={{
              title: 'Activity',
            }}
          />
          <Drawer.Screen
            name="Nested"
            component={NestedScreen}
            options={{
              title: 'Nested',
            }}
          />
          <Drawer.Screen
            name="Masonry"
            component={MasonryScreen}
            options={{
              title: 'Masonry',
            }}
          />
          <Drawer.Screen
            name="Contacts"
            component={ContactsScreen}
            options={{
              title: 'Contacts',
            }}
          />
          <Drawer.Screen
            name="SectionList"
            component={SectionListScreen}
            options={{
              title: 'Section List',
            }}
          />
          <Drawer.Screen
            name="Reorder"
            component={ReorderScreen}
            options={{
              title: 'Reorder',
            }}
          />
          <Drawer.Screen
            name="Tree"
            component={TreeScreen}
            options={{
              title: 'Tree',
            }}
          />
        </Drawer.Navigator>
      </NavigationContainer>
    </SafeAreaProvider>
  );
}
