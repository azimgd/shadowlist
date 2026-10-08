import type { ComponentType } from 'react';
import type { ContactItem } from 'shadowlist-utils/native';
import { FeedScreen } from '@example/screens/FeedScreen';
import { ChatScreen } from '@example/screens/ChatScreen';
import { AssistantScreen } from '@example/screens/AssistantScreen';
import { ActivityScreen } from '@example/screens/ActivityScreen';
import { NestedScreen } from '@example/screens/NestedScreen';
import { MasonryScreen } from '@example/screens/MasonryScreen';
import { ContactsScreen } from '@example/screens/ContactsScreen';
import { SectionListScreen } from '@example/screens/SectionListScreen';
import { ReorderScreen } from '@example/screens/ReorderScreen';
import { ReorderGridScreen } from '@example/screens/ReorderGridScreen';
import { TreeScreen } from '@example/screens/TreeScreen';
import { SnapScreen } from '@example/screens/SnapScreen';
import { TemplatesScreen } from '@example/screens/TemplatesScreen';
import { InboxScreen } from '@example/screens/InboxScreen';
import { ScrollLabScreen } from '@example/screens/ScrollLabScreen';

export type Route =
  | { name: 'Home' }
  | { name: ExampleRoute }
  | { name: 'ContactDetail'; contact: ContactItem };

export type ExampleRoute =
  | 'Feed'
  | 'Chat'
  | 'Assistant'
  | 'Activity'
  | 'Nested'
  | 'Masonry'
  | 'Contacts'
  | 'SectionList'
  | 'Reorder'
  | 'ReorderGrid'
  | 'Tree'
  | 'Snap'
  | 'Inbox'
  | 'ScrollLab'
  | 'Templates';

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
        summary: 'Posts with photo carousels, refreshed from the More menu',
        component: FeedScreen,
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
        route: 'Assistant',
        title: 'Assistant',
        summary: 'Streaming replies that grow in place',
        component: AssistantScreen,
      },
      {
        route: 'Inbox',
        title: 'Inbox',
        summary: 'Select messages and save your place',
        component: InboxScreen,
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
        summary: 'Point at a row to remove it',
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
        summary: 'Click and hold a row, then drag it',
        component: ReorderScreen,
      },
      {
        route: 'ReorderGrid',
        title: 'Sky Wishlist',
        summary: 'Click and hold a card, then drag it across the grid',
        component: ReorderGridScreen,
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
        summary: 'Full-height cards that snap into place',
        component: SnapScreen,
      },
      {
        route: 'ScrollLab',
        title: 'Scroll Lab',
        summary: 'Scroll commands, scroll events and viewability',
        component: ScrollLabScreen,
      },
    ],
  },
  {
    title: 'Components',
    examples: [
      {
        route: 'Templates',
        title: 'Templates',
        summary: 'Every reusable template on one page',
        component: TemplatesScreen,
      },
    ],
  },
];

export const EXAMPLES: Example[] = EXAMPLE_SECTIONS.flatMap(
  (section) => section.examples
);
