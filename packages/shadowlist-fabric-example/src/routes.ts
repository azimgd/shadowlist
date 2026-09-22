import type { ComponentType } from 'react';
import type { ContactItem } from 'shadowlist-utils/native';
import { FeedScreen } from './FeedScreen';
import { FeedNativeScreen } from './FeedNativeScreen';
import { ChatScreen } from './ChatScreen';
import { ChatNativeScreen } from './ChatNativeScreen';
import { AssistantScreen } from './AssistantScreen';
import { ActivityScreen } from './ActivityScreen';
import { NestedScreen } from './NestedScreen';
import { NestedNativeScreen } from './NestedNativeScreen';
import { MasonryScreen } from './MasonryScreen';
import { ContactsScreen } from './ContactsScreen';
import { SectionListScreen } from './SectionListScreen';
import { ReorderScreen } from './ReorderScreen';
import { TreeScreen } from './TreeScreen';
import { SnapScreen } from './SnapScreen';

export type RootStackParamList = {
  Home: undefined;
  ContactDetail: { contact: ContactItem };
} & Record<ExampleRoute, undefined>;

export type ExampleRoute =
  | 'Feed'
  | 'FeedNative'
  | 'Chat'
  | 'ChatNative'
  | 'Assistant'
  | 'Activity'
  | 'Nested'
  | 'NestedNative'
  | 'Masonry'
  | 'Contacts'
  | 'SectionList'
  | 'Reorder'
  | 'Tree'
  | 'Snap';

export interface Example {
  route: ExampleRoute;
  title: string;
  summary: string;
  component: ComponentType;
}

export interface ExampleSection {
  title: string;
  examples: Example[];
}

export const EXAMPLE_SECTIONS: ExampleSection[] = [
  {
    title: 'Feeds',
    examples: [
      {
        route: 'Feed',
        title: 'Feed',
        summary: 'Posts with photo carousels and pull to refresh',
        component: FeedScreen,
      },
      {
        route: 'FeedNative',
        title: 'Native Feed',
        summary: 'The same feed with rows built natively',
        component: FeedNativeScreen,
      },
      {
        route: 'Masonry',
        title: 'Gallery',
        summary: 'Photos in three columns',
        component: MasonryScreen,
      },
      {
        route: 'Nested',
        title: 'Explore',
        summary: 'Horizontal shelves inside a vertical list',
        component: NestedScreen,
      },
      {
        route: 'NestedNative',
        title: 'Native Explore',
        summary: 'Shelves and a grid built natively',
        component: NestedNativeScreen,
      },
    ],
  },
  {
    title: 'Messaging',
    examples: [
      {
        route: 'Chat',
        title: 'Chat',
        summary: 'Group chat that keeps your place as messages arrive',
        component: ChatScreen,
      },
      {
        route: 'ChatNative',
        title: 'Native Chat',
        summary: 'The same chat with bubbles built natively',
        component: ChatNativeScreen,
      },
      {
        route: 'Assistant',
        title: 'Assistant',
        summary: 'Streaming replies that grow in place',
        component: AssistantScreen,
      },
    ],
  },
  {
    title: 'Lists',
    examples: [
      {
        route: 'Activity',
        title: 'Activity',
        summary: 'Opens partway down, loads in both directions',
        component: ActivityScreen,
      },
      {
        route: 'Contacts',
        title: 'Companions',
        summary: 'Swipe a row to remove it',
        component: ContactsScreen,
      },
      {
        route: 'SectionList',
        title: 'Directory',
        summary: 'Sections with sticky headers and an index',
        component: SectionListScreen,
      },
      {
        route: 'Reorder',
        title: 'Boarding Order',
        summary: 'Touch and hold a row, then drag it',
        component: ReorderScreen,
      },
      {
        route: 'Tree',
        title: 'Trip Files',
        summary: 'Folders that expand and collapse',
        component: TreeScreen,
      },
      {
        route: 'Snap',
        title: 'Destinations',
        summary: 'Full-screen cards that snap into place',
        component: SnapScreen,
      },
    ],
  },
];

export const EXAMPLES: Example[] = EXAMPLE_SECTIONS.flatMap(
  (section) => section.examples
);
