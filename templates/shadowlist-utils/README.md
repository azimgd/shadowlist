# ShadowListUtils

ShadowListUtils is the hooks and helpers package for [ShadowList](../../packages/shadowlist-fabric/README.md), plus
themed React Native list kits. Install it from npm as `shadowlist-utils`.

Contents: [Install](#install) · [Usage](#usage) · [API](#api) · [Native kits](#native-kits)

## Install

```sh
yarn add shadowlist shadowlist-utils
```

Choose the import that fits your use case:

| Import | Includes | Extra dependencies |
| --- | --- | --- |
| `shadowlist-utils` | List state, paging, scrolling, grouping and tree helpers | No UI kit peers; React Query v5 is optional for the query examples |
| `shadowlist-utils/native` | Themes, controls, Chat, Assistant and other kits | iOS/Android: Gesture Handler, Reanimated and Safe Area Context |

### Requirements

These are the effective requirements with the current ShadowList release; ShadowListUtils declares broader peer ranges.

| Dependency | Requirement |
| --- | --- |
| `shadowlist` | `*` (use its platform requirements) |
| `react` | `>=19.2.0` |
| `react-native` | `>=0.83.0`, New Architecture |

For iOS/Android kits with Reanimated 4, also install:

```sh
yarn add react-native-gesture-handler react-native-reanimated@^4 react-native-worklets react-native-safe-area-context
```

Use `react-native-worklets/plugin` last in the Babel plugins and wrap the app in `GestureHandlerRootView` and
`SafeAreaProvider`. See the tested [Babel configuration](../shadowlist-fabric-example/babel.config.js).
The root helpers do not need these wrappers.

macOS uses source `.macos` variants without those UI kit peers; see the
[macOS Metro configuration](../shadowlist-macos-example/metro.config.js). Pull to refresh and keyboard avoidance are
unavailable on macOS, and snapping lands without an animation; Inbox keeps selection and its menu opens on a right
click, without swipe actions.

An app outside this repo can depend on a checkout with `"shadowlist-utils": "file:../shadowlist/templates/shadowlist-utils"`.
Metro and TypeScript (`"customConditions": ["react-native"]`) read the TypeScript source. Add the folder to Metro's
`watchFolders` and block its own `node_modules`.

## Usage

```tsx
import { useInfiniteQuery } from '@tanstack/react-query';
import { Text } from 'react-native';
import { ShadowList } from 'shadowlist';
import { shareInfiniteItemsByKey, useInfiniteListProps } from 'shadowlist-utils';

type Post = { id: string; text: string };
type Page = { items: Post[]; nextCursor?: string };

export function FeedScreen({ fetchPage }: { fetchPage: (cursor?: string) => Promise<Page> }) {
  const feed = useInfiniteQuery({
    queryKey: ['feed'],
    queryFn: ({ pageParam }) => fetchPage(pageParam),
    initialPageParam: undefined as string | undefined,
    getNextPageParam: (page: Page) => page.nextCursor,
    structuralSharing: shareInfiniteItemsByKey,
  });

  const list = useInfiniteListProps(feed);

  return (
    <ShadowList
      {...list}
      renderItem={({ item }) => <Text>{item.text}</Text>}
    />
  );
}
```

Install `@tanstack/react-query` for this example and render it inside a `QueryClientProvider`.
See [FeedScreen](../shadowlist-fabric-example/src/screens/FeedScreen.tsx) and its
[query setup](../shadowlist-fabric-example/src/queries/feed.ts) for loading/error UI and cache updates.

## API

The tables cover common exports. Full types and signatures are linked from the
[root exports](src/index.ts) and [native exports](src/native/index.ts).

### Infinite queries

`useInfiniteListProps` memoizes data, defaults refresh to `query.refetch`, and exposes stable edge handlers. They
ignore calls while fetching or when no page remains. `refreshing` tracks pull to refresh only, not background fetches.

| Export | Description |
| --- | --- |
| `useInfiniteListProps(query, { refresh?, onError? })` | Returns `data`, `refreshing`, `onRefresh`, `onEndReached`, `onStartReached`. |
| `flattenInfiniteItems(data)` | Returns all rows of all loaded pages in order. While `data` is undefined it returns one shared empty array. |
| `updateInfiniteItems(data, update)` | Replaces rows with `update(item)`. Return the same row to leave it unchanged. |
| `removeInfiniteItems(data, shouldRemove)` | Removes every row that `shouldRemove` matches. |
| `prependInfiniteItems(data, items)` | Inserts rows at the top of the first page. Only correct while that page is the true start of the list. |
| `appendInfiniteItems(data, items)` | Inserts rows at the bottom of the last page. Only correct while there is no next page. |
| `upsertInfiniteItems(data, items)` | Replaces rows whose `id` is already cached, wherever they are, and appends the rest like `appendInfiniteItems`. Never creates a duplicate key. |
| `trimInfinitePages(data, pageCount)` | Keeps the first `pageCount` pages. Use it before a refetch so that pull to refresh sends one request instead of one per loaded page. |
| `shareInfiniteItemsByKey(previous, next)` | Structural sharing for infinite data (`structuralSharing` option). A row keeps its previous object when it equals the previous row by value with the same `id`, even if it moved. |
| `shareItemsByKey(previous, next)` | Same as above, but also works on a plain array query (`useQuery`). |

All edit helpers keep the identity of any page or row they don't change. They return `data` itself when nothing changed, and they pass `undefined` through.

Use edit helpers inside `queryClient.setQueryData`. For optimistic updates and retries of failed messages, see
[chat mutations](../shadowlist-fabric-example/src/queries/chat.ts).

### List state and scrolling

`useListController` holds local data, loading flags and scrolling state. Its options include initial data, a
`keyExtractor` (default `item.id`), refresh/page/scroll callbacks, `onError` and `scrollIdleMs` (150 ms).
Use `setItems`, `prependItems`, `appendItems`, `upsertItems`, `updateItem` and `removeItems` to edit data; `markers`
set loading/scrolling flags manually. See [controller types](src/useListController.ts) for callback signatures.

| Export | Description |
| --- | --- |
| `useListController<ItemT>(options?)` | Local list state, edit methods and handlers that guard concurrent loads. |
| `usePullToRefresh(refresh, { onError? })` → `{ refreshing, onRefresh }` | Drives the pull to refresh control from any async function. `refresh` is read when called. It doesn't need to be stable. |
| `useScrollThreshold(thresholdDp)` → `{ isPastThreshold, onScroll }` | Renders again only when the scroll offset crosses the threshold, for example to hide a header or show a button that scrolls back to the top. |
| `getViewableRange(viewableItems)` → `{ low, high } \| undefined` | Returns the index span of an `onViewableItemsChanged` payload. The payload doesn't need to be sorted. |

For local data, connect `data`, `handleRefresh`, `handleStartReached` and `handleEndReached` from
`useListController` to the corresponding list props. See
[AssistantScreen](../shadowlist-fabric-example/src/screens/AssistantScreen.tsx) for local state.

### Sections and trees

| Export | Description |
| --- | --- |
| `groupIntoSections(items, { getSectionTitle, compareSections?, compareItems? })` → `{ key, title, data }[]` | Groups a flat array into SectionList sections. Wrap the call in `useMemo`. |
| `collectExpandableKeys(nodes, { getChildren, keyExtractor })` → `string[]` | Returns the key of every node that has children, at any depth. Use it for a TreeList "Expand all" (`expandedKeys`). |

## Native kits

Import kits from `shadowlist-utils/native`, for example `Chat.List` and `Chat.Bubble`. Lists forward the props allowed
by their exported `*ListProps` type and expose the underlying list commands through `ref` (`Tree.List` uses
`TreeListCommands`, and `Board.List` takes no `ref`). Most provide a default row that an optional `renderItem` can replace. `Poll.List` takes `poll`
instead of `data`; `Contacts.SectionList` takes `sections`; `Nested.Row` has no list wrapper. `TemplateList` exposes
its smaller prop set and a `listRef` prop.

### Theming

```tsx
import { ThemeProvider, createTheme, darkTheme } from 'shadowlist-utils/native';

const theme = createTheme(darkTheme, { colors: { accent: '#ff7a00' } });
<ThemeProvider theme={theme}><App /></ThemeProvider>;
```

See [app theming](../shadowlist-fabric-example/src/appTheme.ts) for the full theme and cached `createStyles` setup.

Without a provider, `useTheme` follows the system color scheme. With a provider, the app chooses its theme.
Overrides merge deeply; arrays such as `avatarPalette` are replaced. Styles are cached per theme object.

A few tokens choose a platform's idiom rather than a metric. Each default draws as iOS does.

| Token | Default | Effect |
| --- | --- | --- |
| `grouped.captionCase` | `'upper'` | `'none'` keeps the titles of `Grouped.Caption` and `SectionHeader` as written. |
| `grouped.chevron` | `true` | `false` drops the chevron from a `Grouped.Row` that does not set `chevron`. |
| `pressFeedback` | `'highlight'` | `'ripple'` gives `Grouped.Row`, `PillButton` and `IconButton` Android's ripple in `colors.fill` instead of the pressed fill or dimming. Other platforms keep the highlight. List rows keep it everywhere because they mount in recycled cells. |

| Export | Description |
| --- | --- |
| `ThemeProvider({ theme })` | Provides a theme to the tree. |
| `useTheme()` | Returns the current [Theme](src/native/theme/Theme.ts). |
| `createTheme(base, overrides)` | Applies partial overrides to a base theme. |
| `createStyles(factory)` → `useStyles` | Builds a hook that creates styles from the theme and caches them per theme object. |
| `lightTheme`, `darkTheme` | The included themes. |

### Labels (i18n)

All visible and accessibility strings come from a `labels` prop. You pass a `Partial` of the kit's labels type, and anything you leave out falls back to the English defaults (`defaultChatLabels`, `defaultFeedLabels`, `defaultAssistantLabels` and so on). Some labels are functions, for example `minutes: (n) => \`${n} min\``.

```tsx
const LABELS = { placeholder: 'Nachricht', send: 'Senden' }; // module scope or useMemo
<Chat.Input onSend={send} labels={LABELS} />;
```

Merged labels keep the same identity while their values don't change. An inline object won't render memoized rows again.

| Export | Description |
| --- | --- |
| `useLabels(defaults, overrides)` | Merges a `Partial` of labels into the defaults. |
| `formatRelativeTime(date, labels, nowMs?)` | Formats a relative time with the given labels. |

### Primitives

| Export | Description |
| --- | --- |
| `Avatar({ name, uri?, color?, size? })` | Shows initials, with the image on top when `uri` is set. The color is derived from `name`. |
| `Spinner({ size?, color?, labels? })`, `defaultSpinnerLabels` | Centered activity indicator. |
| `ListHeader({ title, subtitle? })`, `ListFooter({ text })` | Header and footer blocks for lists. |
| `SectionHeader({ title, count? })`, `ItemSeparator({ highlighted? })` | Section header and separator line. |
| `TemplateList({ data, templates, horizontal?, listRef? })` (`TemplateRow`, `TemplateMap`) | A ShadowList of mixed rows. Each row has a `kind`, and `templates` maps each kind to its renderer. Selection, keyboard selection, swipe actions, menus, `contentContainerStyle`, `persistentKeys`, `ListHeaderComponent`, `onScroll`, `scrollEventThrottle` and `keyboardDismissMode` pass through to the list. |
| `useLargeText(threshold = 1.3)` | True at accessibility text sizes. Shares one `Dimensions` listener across callers. |
| `useKeyboardVisible()` | True while the keyboard is on screen. |
| `useKeyboardLift({ gap? })` | Returns an animated `translateY` that lifts a composer above the keyboard. |

See [native exports](src/native/index.ts) for icons and their prop types.

### Controls

| Export | Description |
| --- | --- |
| `PillButton({ label, onPress, variant?, disabled? })` | `primary`, `tinted`, `plain` or `destructive` pill. |
| `IconButton({ icon, onPress, accessibilityLabel })` | Round tinted button around a glyph. |
| `DashedButton({ label, onPress, disabled? })` | Dashed outline across the full width that adds one more of something. |
| `Chip({ label, onPress, variant?, mono? })`, `ChipRow` | Shortcut chips in a row that wraps. |
| `ChoiceChip({ label, tint, selected, onPress })` | One choice in its own color, ringed and ticked when selected. |
| `SearchField({ value, onChange, icon?, labels? })` | Filled search box. `defaultSearchFieldLabels`. |
| `TextField({ value, onChange, placeholder?, multiline? })` | Filled text box for a value typed verbatim. |
| `Segmented({ options, value, onChange, size? })` | Drawn segmented control. `compact` sizes it to its labels. |
| `ProgressBar({ fraction, accessibilityLabel, color?, height? })` | Rounded bar with a progressbar role. |
| `ProgressRing({ fraction, size?, strokeWidth?, color? })` | Ring that fills clockwise. Hidden from screen readers. |
| `IndexBadge({ index })` | Position in an ordered list. |
| `EmptyState({ icon?, title, message? })` | What a screen says before it has anything to show. |

### Grouped and Form

Inset grouped cards, as iOS Settings draws them. Metrics come from `theme.grouped`. Cards paint `colors.groupedCell`. Put them on a page of `colors.groupedBackground`.

```tsx
<Grouped.Section title="Account">
  <Grouped.Row title="Plan" subtitle="Pro" onPress={openPlan} />
</Grouped.Section>

<Form.Card>
  <Form.Row label="Amount">
    <Form.Input numeric value={amount} onChange={setAmount} accessibilityLabel="Amount" />
  </Form.Row>
</Form.Card>
```

See [TemplatesScreen](../shadowlist-fabric-example/src/screens/TemplatesScreen.tsx) for imports, controls and handlers.

| Export | Description |
| --- | --- |
| `Grouped.Card`, `Grouped.Caption({ title })`, `Grouped.Section({ title? })` | A card, the caption over it (uppercase unless `grouped.captionCase` is `'none'`), and the two together. |
| `Grouped.Row({ title, subtitle?, leading?, trailing?, chevron?, separated?, onLongPress?, longPressLabel?, testID? })` | A row that leads somewhere. The long press is also a screen reader action. Swipe actions and a menu work as in `Grouped.Actions`. |
| `Grouped.Actions({ leadingSwipeActions?, trailingSwipeActions?, contextMenu? })` | Gives a custom card row the swipe actions and menu of a list row: a swipe on iOS and Android, a right click on macOS. A destructive action takes the theme's red. The row needs its own background. |
| `Grouped.Separator({ inset? })` | Hairline that starts at `inset`, the row inset by default. |
| `Form.Card`, `Form.Row({ label, stacked? })` | A form card that draws its own separators, and a row with a label and a value. Large text stacks the label. |
| `Form.Input({ value, onChange, numeric?, keyboardType?, autoCapitalize?, autoCorrect?, returnKeyType?, onSubmitEditing?, accessibilityLabel })`, `Form.Value({ text })` | A row's field, aligned to the trailing edge, and a value it shows but does not take. An `email-address` keyboard turns off correction and capitals unless asked. |
| `Form.Button({ label, onPress, destructive? })` | Button across the full width, in a card of its own. |

### Chat

`Chat.List` is inverted, takes messages oldest first and predicts the sizes of text bubbles. This minimal screen uses
messages the app owns and a send callback:

```tsx
import { View } from 'react-native';
import { Chat, type ChatMessage } from 'shadowlist-utils/native';

export function ChatScreen({ messages, send }: {
  messages: ChatMessage[];
  send: (text: string) => void;
}) {
  return (
    <View style={{ flex: 1 }}>
      <Chat.List data={messages} />
      <Chat.Input onSend={send} />
    </View>
  );
}
```

`Chat.List` handles `onRetryMessage` and `onLongPressMessage`; `Chat.Bubble` offers `onRetry(message)` and
`onLongPress`. The status of your own messages is `sending`, `sent`, `delivered`, `read` or `failed`; failed messages add a retry
line. `showsAuthor={false}` hides the name and avatar of a message from someone else and keeps the avatar's place,
for the rest of a run from one author. `renderText(text)` draws the text as nested runs, such as mentions, and
`children` draw inside the bubble under the text. If you replace the row, supply a matching `getChatMessageSizeSpec`,
including any caption and `showsAuthor`. The spec predicts the text and the bubble around it, not `children`: a
bubble with children takes its measured size once it mounts.
See [ChatScreen](../shadowlist-fabric-example/src/screens/ChatScreen.tsx) for paging, keyboard space, captions and retry.
[Chat types](src/native/chat/types.ts) define messages, authors and statuses.

### Assistant (streaming AI chat)

During streaming, append a reply to `data` once, send tokens to `createTurnWriter`, then commit the final turn once.
Every mounted reply subscribes to the store and only the streaming one renders again; tokens flush every `flushMs`
(50 ms by default).

```tsx
import { useMemo } from 'react';
import { Assistant, createStreamStore, type AssistantMessage } from 'shadowlist-utils/native';

export function AssistantHistory({ messages }: { messages: AssistantMessage[] }) {
  const store = useMemo(() => createStreamStore(), []);
  return <Assistant.List data={messages} store={store} />;
}
```

This renders saved messages. For streaming, follow
[AssistantScreen](../shadowlist-fabric-example/src/screens/AssistantScreen.tsx): `startReply` commits in `onFinish`
and queues the finished message id; the effect keyed by `list.data` then calls `store.remove(id)` **after the data
commit**. Its unmount effect stops the active stream and clears timers. Keep the store stable and cancel the
transport as well as finishing the writer when stopping a reply.

| Export | Purpose |
| --- | --- |
| `Assistant.List` | Message list; requires `store`, accepts `streaming` and reply callbacks. Keeps the end in view as replies arrive and grow, `onAtEndChange` reports a reader who scrolled away, and a drag dismisses the keyboard |
| `Assistant.Composer` | Send/Stop, attachments and editing; ref supports `setDraft`, `clearDraft`, `focus`. Pass `value` and `onChangeText` to own the draft, and `accessory` for a control in its toolbar |
| `createStreamStore`, `useStreamingTurn` | Store lifecycle and a subscription for each message |
| `createTurnWriter` | Batched thinking/content, tool calls, completion, failure and stop |
| `emptyTurn`, `ASSISTANT_END_MARKER`, `ASSISTANT_END_ID` | Initial turn, and an invisible row to append after the last message whose viewability shows the end is on screen. `Assistant.List` does not need it |

`Assistant.ReplyMessage`, `Assistant.UserMessage`, `Assistant.Markdown` and the other building blocks support custom
rows. See [Assistant exports](src/native/assistant/index.ts) and [writer methods](src/native/assistant/turnWriter.ts)
for exact signatures. Calls after a writer finishes are ignored.

#### Links in replies

Markdown links and source cards open only `http:`, `https:` and `mailto:` by default. `onOpenLink` replaces that
behavior completely; use exported `openUrl` as the fallback when handling a custom scheme:

```tsx
import { openUrl } from 'shadowlist-utils/native';

const onOpenLink = (url: string) =>
  url.startsWith('myapp://trip/') ? openTrip(url) : openUrl(url);
```

Pass this callback to `Assistant.List`, `Assistant.ReplyMessage` or `Assistant.Markdown`. Validate links in your
custom handler before opening them.

### Board (drag between columns)

`Board.List` lays columns side by side in a horizontal scroll that snaps to each column. Every column is a
`DraggableList` on one [drag board](../../packages/shadowlist-fabric/README.md#drag-boards), with a header that shows
its title and count and a "Drop cards here" hint while it is empty. A card moves within its column or into another
one. Boards are implemented on iOS, Android and macOS and have been exercised on iOS. `useDragBoardState` holds the columns and applies each move:

```tsx
import { Board, useDragBoardState, type BoardColumn } from 'shadowlist-utils/native';

type Task = { id: string; title: string };

export function TaskBoard({ initialColumns }: { initialColumns: BoardColumn<Task>[] }) {
  const board = useDragBoardState(initialColumns);
  return (
    <Board.List
      columns={board.columns}
      renderItem={({ item }) => <Board.Card title={item.title} />}
      onMoveItem={board.moveItem}
    />
  );
}
```

The board names its drag board with `useId` and each list from the board and the column key. Two boards in one React
root never join by accident, and no list goes without an id. `idPrefix` only makes the ids readable. A column's title names it in
the Move to action of VoiceOver and TalkBack. Without `onMoveItem` the cards stay put, and an empty column says "No
cards" instead of where to drop them.

`renderColumnFooter(column)` draws a view under a column's cards, such as an add row. It sits outside the column's
list, so it never lifts, never drags and takes no place among the cards. A card let go over it lands in that column
where its gap last showed, which is the end for a card brought down past the last one. A drag board cannot keep
one row from lifting: a card that must stay put either stays out of `items` or has its move refused in
`onMoveItem`. `renderColumnHeader(column)` takes the default header's place, and then the header's accessibility
label is yours to give. `columnStyle` styles every column, for a tinted background or other padding.

A column takes 78% of the board, at most 360 points, with the next column's edge in view. `columnWidth` sets every
column's width in points instead, for a board that lines its columns up with the rest of a page, and the snap to
each column follows it. A column narrower than 78% of the board keeps 24 points of padding before the first column,
and one as wide or wider is centred. `sidePadding` sets that inset in points instead, before the first column and
after the last, so the first column starts at the page's own gutter. A `columnWidth` that is not a positive number
and a `sidePadding` that is negative or not finite are ignored.

By default the columns fill the height the board is given. A board inside a page sets `columnHeight="content"`
and sizes itself to its cards: every column's cards are as tall as the tallest column's, measured by its list,
between `minColumnHeight` (120) and `maxColumnHeight` (60% of the window height, of the screen on macOS), so each column is a drop target down the
whole board. A column taller than that scrolls inside, and one that fits does not, so a swipe on it scrolls the
page. Until a column has measured its cards, each counts `estimatedItemHeight` (64).

| Export | Description |
| --- | --- |
| `Board.List({ columns, renderItem, keyExtractor?, onMoveItem?, idPrefix?, columnWidth?, sidePadding?, columnHeight?, minColumnHeight?, maxColumnHeight?, estimatedItemHeight?, columnStyle?, renderColumnHeader?, renderColumnFooter?, labels?, style?, testID? })` | The board. `columns` are `BoardColumn`s, `{ key, title, items }`. `onMoveItem(BoardMoveInfo)` gets a reorder and a drop into another column in one shape, `{ item, sourceColumnKey, destinationColumnKey, destinationKey, destinationIndex }`. `keyExtractor` defaults to `item.id`. `columnWidth` (points, used as it is for the columns and the snap) defaults to 78% of the board, at most 360. `sidePadding` (points, before the first column and after the last) defaults to 24, or centres a column as wide as 78% of the board. `columnHeight` is `'fill'` (the default) or `'content'`. |
| `Board.Card({ title, label?, detail?, color?, accessibilityLabel? })` | A card with an optional colored bar and a label and detail under the title. Its cell stays transparent, and a picked up card lifts alone. |
| `useDragBoardState(initialColumns, { keyExtractor?, onMoveItem? })` → `{ columns, setColumns, moveItem }` | Holds the columns. `moveItem` is stable and updates both columns of a drop in one state change. `onMoveItem` hears each move once the state change is queued, before the board renders it, to save it. |
| `moveBoardItem(columns, move, keyExtractor?)` | Applies one move. The item leaves every column that holds its key and goes before `destinationKey`, at the end for `''`, or at `destinationIndex` for a key no longer there. Untouched columns keep their identity, and the board comes back as is when nothing moved. |

Labels: `defaultBoardLabels` (`emptyColumn`, `emptyColumnReadOnly`, `columnSummary(title, count)`). See
[BoardScreen](../shadowlist-fabric-example/src/screens/BoardScreen.tsx).

### Other kits

| Kit | Input / behavior | Example / source |
| --- | --- | --- |
| `Feed.List` | `FeedItem[]`; posts and images | [FeedScreen](../shadowlist-fabric-example/src/screens/FeedScreen.tsx) |
| `Activity.List` | `ActivityItem[]`; sticky header/footer and separators | [ActivityScreen](../shadowlist-fabric-example/src/screens/ActivityScreen.tsx) |
| `Nested.Row` | `NestedItem` with horizontal cards | [NestedScreen](../shadowlist-fabric-example/src/screens/NestedScreen.tsx) |
| `Masonry.List` | `MasonryItem[]`; three columns by default | [MasonryScreen](../shadowlist-fabric-example/src/screens/MasonryScreen.tsx) |
| `Contacts.List`, `Contacts.SectionList` | `ContactItem[]` or sections; `onDelete` enables swipe to delete | [ContactsScreen](../shadowlist-fabric-example/src/screens/ContactsScreen.tsx) |
| `Reorder.List` | `ContactItem[]`; save `onMoveItem` data | [ReorderScreen](../shadowlist-fabric-example/src/screens/ReorderScreen.tsx) |
| `Tree.List` | `TreeNode[]`; controlled expansion via `expandedKeys` / `onExpandedChange` | [TreeScreen](../shadowlist-fabric-example/src/screens/TreeScreen.tsx) |
| `Poll.List` | `poll: { question, options, selectedId }` and `onVote` | [PollListProps](src/native/poll/PollList.tsx) |
| `Snap.List` | `SnapItem[]`; `snapToItem` | [SnapScreen](../shadowlist-fabric-example/src/screens/SnapScreen.tsx) |
| `Inbox.List` | `InboxMessage[]`; read/flag/delete and controlled selection | [InboxScreen](../shadowlist-fabric-example/src/screens/InboxScreen.tsx) |
