# shadowlist-utils

Hooks, data helpers and ready-made list kits for [`shadowlist`](https://github.com/azimgd/shadowlist).
`shadowlist-utils` has the plain hooks and helpers. `shadowlist-utils/native` has themed UI kits for feeds, chats,
assistants, inboxes and more.

## Install

```sh
yarn add shadowlist shadowlist-utils
# UI kits (shadowlist-utils/native):
yarn add react-native-gesture-handler react-native-reanimated react-native-safe-area-context
```

- `react` 18.2 or newer and `react-native` 0.74 or newer with the New Architecture (Fabric).
- `shadowlist-utils/native` needs the three peers above. Add the Reanimated Babel plugin and wrap the app in
  `GestureHandlerRootView` and `SafeAreaProvider`.
- The infinite query helpers work on `@tanstack/react-query` data. It is not a dependency.

An app outside this repo can depend on a checkout with `"shadowlist-utils": "file:../shadowlist/templates/shadowlist-utils"`.
Metro and TypeScript (`"customConditions": ["react-native"]`) read the TypeScript source. Add the folder to Metro's
`watchFolders` and block its own `node_modules`.

## Usage

```tsx
import { useInfiniteQuery } from '@tanstack/react-query';
import { ShadowList } from 'shadowlist';
import { shareInfiniteItemsById, useInfiniteListProps } from 'shadowlist-utils';

type Post = { id: string; text: string };
type Page = { items: Post[]; nextCursor?: string };

export function FeedScreen() {
  const feed = useInfiniteQuery({
    queryKey: ['feed'],
    queryFn: ({ pageParam }) => fetchFeedPage(pageParam), // resolves to Page
    initialPageParam: undefined as string | undefined,
    getNextPageParam: (page: Page) => page.nextCursor,
    // Rows keep their identity when pages shift. Only new rows render.
    structuralSharing: shareInfiniteItemsById,
  });

  const list = useInfiniteListProps(feed, {
    onError: (error) => console.warn('page load failed', error),
  });

  if (feed.data === undefined) return null;

  return (
    <ShadowList
      data={list.data}
      refreshing={list.refreshing}
      onRefresh={list.onRefresh}
      onEndReached={list.onEndReached}
      onStartReached={list.onStartReached}
      renderElement={({ element }) => <PostRow post={element} />}
    />
  );
}
```

Pages must have the shape `{ items: Item[] }`. Rows are matched by `id`.

## API

### Error handling (`onError`)

`useListController`, `usePullToRefresh` and `useInfiniteListProps` each take an `onError` option. It receives a synchronous throw or a rejected promise from the callback you passed in (`onRefresh` / `onEndReached` / `onStartReached`, `refresh`, or a page fetch). Without `onError`, the rejection is left **unhandled**. In both cases the loading flag (`refreshing`, `loadingMore`, `loadingOlder`) and the re-entrancy guard reset. The list never gets stuck in a loading state.

React Query's own `fetchNextPage` and `refetch` resolve even when the request fails, and the error ends up on the query. So `onError` matters mostly when you pass a custom `refresh` or your own fetchers.

### Infinite queries

| Export                                                                                                                                      | Description                                                                                                                                                                                                                                                                                                                                           |
| ------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `useInfiniteListProps(query, { refresh?, onError? })` → `{ data, refreshing, onRefresh, onEndReached, onStartReached }`                     | Connects an infinite query to a list. `data` is memoized on the cached value. The end and start handlers are stable and do nothing when there is no page to load, while any fetch is running, or while their own fetch is still pending. `refreshing` only tracks pull-to-refresh, never background refetches. `refresh` defaults to `query.refetch`. |
| `flattenInfiniteItems(data)`                                                                                                                | Returns all rows of all loaded pages in order. While `data` is undefined it returns one shared empty array.                                                                                                                                                                                                                                           |
| `updateInfiniteItems(data, update)`                                                                                                         | Replaces rows with `update(item)`. Return the same row to leave it unchanged.                                                                                                                                                                                                                                                                         |
| `removeInfiniteItems(data, shouldRemove)`                                                                                                   | Removes every row that `shouldRemove` matches.                                                                                                                                                                                                                                                                                                        |
| `prependInfiniteItems(data, items)`                                                                                                         | Inserts rows at the top of the first page. Only correct while that page is the true start of the list.                                                                                                                                                                                                                                                |
| `appendInfiniteItems(data, items)`                                                                                                          | Inserts rows at the bottom of the last page. Only correct while there is no next page.                                                                                                                                                                                                                                                                |
| `upsertInfiniteItems(data, items)`                                                                                                          | Replaces rows whose `id` is already cached, wherever they are, and appends the rest like `appendInfiniteItems`. Never creates a duplicate key.                                                                                                                                                                                                        |
| `trimInfinitePages(data, pageCount)`                                                                                                        | Keeps the first `pageCount` pages. Use it before a refetch so that pull-to-refresh sends one request instead of one per loaded page.                                                                                                                                                                                                                  |
| `shareInfiniteItemsById(previous, next)`                                                                                                    | Structural sharing for infinite data (`structuralSharing` option). A row keeps its previous object when it is value-equal to the previous row with the same `id`, even if it moved.                                                                                                                                                                   |
| `shareItemsById(previous, next)`                                                                                                            | Same as above, but also works on a plain array query (`useQuery`).                                                                                                                                                                                                                                                                                    |
| Types: `ItemsPage<T>`, `InfinitePages<P>`, `InfiniteItem<D>`, `InfiniteListQuery<D>`, `InfiniteListProps<T>`, `UseInfiniteListPropsOptions` |                                                                                                                                                                                                                                                                                                                                                       |

All edit helpers keep the identity of any page or row they don't change. They return `data` itself when nothing changed, and they pass `undefined` through.

Optimistic edits go through `setQueryData`:

```ts
import { upsertInfiniteItems } from 'shadowlist-utils';

const write = (message: ChatMessage) =>
  queryClient.setQueryData<ChatData>(['chat'], (data) =>
    upsertInfiniteItems(data, [message])
  );

useMutation({
  mutationFn: sendMessage,
  onMutate: (message) => write({ ...message, status: 'sending' }),
  onSuccess: (stored) => write(stored),
  // Keep the bubble so the text isn't lost; Chat.Bubble offers a retry.
  onError: (_error, message) => write({ ...message, status: 'failed' }),
});
```

### List state and scrolling

| Export                                                                                                                                                                  | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `useListController<T extends { id: string }>(options?)`                                                                                                                 | Holds local list state: `data`, `refreshing`, `loadingMore`, `loadingOlder`, `scrolling`. It returns handlers that are safe to call again while busy (`handleRefresh`, `handleEndReached`, `handleStartReached`, `handleScroll`, `handleViewableItemsChanged`), mutators (`setData`, `prepend`, `append`, `upsertItems(items)`, `updateItem(id, update)`, `removeItems(ids \| predicate)`) and manual `markers`. Options: `initialData`, `onRefresh`, `onEndReached`, `onStartReached`, `onScroll`, `onViewableItemsChanged`, `onError`, `scrollIdleMs` (default 150). |
| `usePullToRefresh(refresh, { onError? })` → `{ refreshing, onRefresh }`                                                                                                 | Drives the pull-to-refresh control from any async function. `refresh` is read when called. It doesn't need to be stable.                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `useScrollThreshold(thresholdDp)` → `{ isPastThreshold, onScroll }`                                                                                                     | Re-renders only when the scroll offset crosses the threshold, for example to hide a header or show a back-to-top button.                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `getViewableRange(viewableItems)` → `{ firstIndex, lastIndex } \| undefined`                                                                                            | Returns the index span of an `onViewableItemsChanged` payload. The payload doesn't need to be sorted.                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Types: `UseListControllerOptions`, `ListController`, `ListMarkers`, `PullToRefresh`, `UsePullToRefreshOptions`, `ScrollThreshold`, `ScrollOffsetEvent`, `ViewableRange` |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |

```tsx
const list = useListController<Message>({
  onStartReached: async () => list.prepend(await fetchOlder()),
  onError: reportError,
});
<ShadowList data={list.data} onStartReached={list.handleStartReached} ... />;
```

### Sections and trees

| Export                                                                                                      | Description                                                                                                         |
| ----------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `groupIntoSections(items, { getSectionTitle, compareSections?, compareItems? })` → `{ key, title, data }[]` | Groups a flat array into `SectionList` sections. Wrap the call in `useMemo`.                                        |
| `collectExpandableIds(nodes, { getChildren, keyExtractor })` → `string[]`                                   | Returns the id of every node that has children, at any depth. Use it for a `TreeList` "Expand all" (`expandedIds`). |
| Types: `ItemSection<T>`, `GroupIntoSectionsOptions<T>`, `CollectExpandableIdsOptions<N>`                    |                                                                                                                     |

## Native kits

Each kit is a namespace object, such as `Chat.List` and `Chat.Bubble`. Every `*.List` component wraps `ShadowList` (or `SectionList`, `TreeList` or `DraggableList`), accepts all of that component's props and forwards `ShadowListCommands` through `ref`. It also takes an optional `renderElement` that replaces the kit's default row.

### Theming

```tsx
import {
  ThemeProvider,
  createTheme,
  darkTheme,
  lightTheme,
  createStyles,
} from 'shadowlist-utils/native';

const brandDark = createTheme(darkTheme, { colors: { accent: '#ff7a00' } });

<ThemeProvider theme={scheme === 'dark' ? brandDark : lightTheme}>
  <App />
</ThemeProvider>;

// Styles are cached per theme object.
const useStyles = createStyles(({ colors, spacing }) =>
  StyleSheet.create({
    list: { flex: 1, backgroundColor: colors.background, padding: spacing.md },
  })
);
```

| Export                                                                                                                                                                                                   | Description                                                                                                                                                                                                             |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ThemeProvider({ theme })`                                                                                                                                                                               | Provides a `Theme` to the tree. Without a provider, `useTheme` follows the system color scheme and switches between `lightTheme` and `darkTheme`. With a provider, choosing light or dark is up to you.                 |
| `useTheme()`                                                                                                                                                                                             | Returns the current `Theme`: `colors`, `typography`, `fontSize`, `fontWeight`, `spacing`, `radius`, `fonts`, `grouped` (`inset`, `radius`, `rowInset`, `rowHeight` of inset-grouped cards), `rowInset` and `tapTarget`. |
| `createTheme(base, overrides)`                                                                                                                                                                           | Deep-merges a `DeepPartial<Theme>` into a base theme. Arrays such as `avatarPalette` are replaced, not merged.                                                                                                          |
| `createStyles(factory)` → `useStyles`                                                                                                                                                                    | Builds a hook that creates styles from the theme and caches them per theme object.                                                                                                                                      |
| `lightTheme`, `darkTheme`                                                                                                                                                                                | Built-in themes.                                                                                                                                                                                                        |
| Types: `Theme`, `ThemeColors`, `ThemeTextStyle`, `ThemeTypography`, `ThemeFontSize`, `ThemeFontWeight`, `ThemeSpacing`, `ThemeRadius`, `ThemeFonts`, `ThemeGrouped`, `DeepPartial`, `ThemeProviderProps` |                                                                                                                                                                                                                         |

### Labels (i18n)

All user-visible and accessibility strings come from a `labels` prop. You pass a `Partial` of the kit's labels type, and anything you leave out falls back to the English defaults (`defaultChatLabels`, `defaultFeedLabels`, `defaultAssistantLabels` and so on). Some labels are functions, for example `minutes: (n) => \`${n} min\``.

```tsx
const LABELS = { placeholder: 'Nachricht', send: 'Senden' }; // module scope or useMemo
<Chat.Input onSend={send} labels={LABELS} />;
```

Merged labels keep the same identity while their values don't change. An inline object won't re-render memoized rows.
Related exports: `useLabels(defaults, overrides)`, `formatRelativeTime(date, labels, nowMs?)` and `RelativeTimeLabels`.

### Primitives

| Export                                                                                                                                                                                                                                     | Description                                                                                          |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| `Avatar({ name, uri?, color?, size? })`                                                                                                                                                                                                    | Shows initials, with the image on top when `uri` is set. The color is derived from `name`.           |
| `Spinner({ size?, color?, labels? })`, `defaultSpinnerLabels`                                                                                                                                                                              | Centered activity indicator.                                                                         |
| `ListHeader({ title, subtitle? })`, `ListFooter({ text })`                                                                                                                                                                                 | Header and footer blocks for lists.                                                                  |
| `SectionHeader({ title, count? })`, `ItemSeparator({ highlighted? })`                                                                                                                                                                      | Section header and separator line.                                                                   |
| `TemplateList({ data, templates, horizontal?, listRef? })` (`TemplateRow`, `TemplateMap`)                                                                                                                                                  | A `ShadowList` of mixed rows. Each row has a `kind`, and `templates` maps each kind to its renderer. |
| `useLargeText(threshold = 1.3)`                                                                                                                                                                                                            | True at accessibility text sizes. Shares one `Dimensions` listener across callers.                   |
| `useKeyboardVisible()`                                                                                                                                                                                                                     | True while the keyboard is on screen.                                                                |
| `useKeyboardLift({ gap? })`                                                                                                                                                                                                                | Returns an animated `translateY` that lifts a composer above the keyboard.                           |
| Icons: `ChevronIcon`, `FolderIcon`, `DocIcon`, `GripIcon`, `ArrowUpIcon`, `PlusIcon`, `CloseIcon`, `StopIcon`, `CheckIcon`, `CopyIcon`, `RetryIcon`, `ShareIcon`, `SparkleIcon`, `PencilIcon`, `SearchIcon` (`IconProps`, `IconComponent`) |                                                                                                      |

### Controls

| Export                                                           | Description                                                   |
| ---------------------------------------------------------------- | ------------------------------------------------------------- |
| `PillButton({ label, onPress, variant?, disabled? })`            | `primary`, `tinted`, `plain` or `destructive` pill.           |
| `IconButton({ icon, onPress, accessibilityLabel })`              | Round tinted button around a glyph.                           |
| `DashedButton({ label, onPress, disabled? })`                    | Full-width dashed outline that adds one more of something.    |
| `Chip({ label, onPress, variant?, mono? })`, `ChipRow`           | Shortcut chips in a row that wraps.                           |
| `ChoiceChip({ label, tint, selected, onPress })`                 | One choice in its own color, ringed and ticked when selected. |
| `SearchField({ value, onChange, icon?, labels? })`               | Filled search box. `defaultSearchFieldLabels`.                |
| `TextField({ value, onChange, placeholder?, multiline? })`       | Filled text box for a value typed verbatim.                   |
| `Segmented({ options, value, onChange, size? })`                 | Drawn segmented control. `compact` sizes it to its labels.    |
| `ProgressBar({ fraction, accessibilityLabel, color?, height? })` | Rounded bar with a progressbar role.                          |
| `ProgressRing({ fraction, size?, strokeWidth?, color? })`        | Ring that fills clockwise. Hidden from screen readers.        |
| `IndexBadge({ index })`                                          | Position in an ordered list.                                  |
| `EmptyState({ icon?, title, message? })`                         | What a screen says before it has anything to show.            |

### Grouped and Form

Inset-grouped cards, as iOS Settings draws them. Metrics come from `theme.grouped`. Cards paint `colors.groupedCell`. Put them on a page of `colors.groupedBackground`.

```tsx
<Grouped.Section title="Account">
  <Grouped.Row title="Plan" subtitle="Pro" onPress={openPlan} />
  <Grouped.Row title="Sign Out" separated chevron={false} onPress={signOut} />
</Grouped.Section>

<Form.Card>
  <Form.Row label="Amount">
    <Form.Input numeric value={amount} onChange={setAmount} accessibilityLabel="Amount" />
  </Form.Row>
  <Form.Row label="Total">
    <Form.Value text={total} />
  </Form.Row>
</Form.Card>
<Form.Button label="Remove item" destructive onPress={remove} />
```

| Export                                                                                                                 | Description                                                                                        |
| ---------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `Grouped.Card`, `Grouped.Caption({ title })`, `Grouped.Section({ title? })`                                            | A card, the uppercase caption over it, and the two together.                                       |
| `Grouped.Row({ title, subtitle?, leading?, trailing?, chevron?, separated?, onLongPress?, longPressLabel?, testID? })` | A row that leads somewhere. The long press is also a screen reader action.                         |
| `Grouped.Separator({ inset? })`                                                                                        | Hairline that starts at `inset`, the row inset by default.                                         |
| `Form.Card`, `Form.Row({ label, stacked? })`                                                                           | A form card that draws its own separators, and a label-and-value row. Large text stacks the label. |
| `Form.Input({ value, onChange, numeric?, accessibilityLabel })`, `Form.Value({ text })`                                | A row's field, aligned to the trailing edge, and a value it shows but does not take.               |
| `Form.Button({ label, onPress, destructive? })`                                                                        | Full-width button in a card of its own.                                                            |

### Chat

`Chat.List` is inverted and predicts the height of text bubbles with `getChatMessageSizeSpec`.

```tsx
import { KeyboardView } from 'shadowlist';
import { useInfiniteListProps } from 'shadowlist-utils';
import {
  Chat,
  getChatMessageSizeSpec,
  useKeyboardLift,
  useTheme,
  type ChatMessage,
} from 'shadowlist-utils/native';

export function ChatScreen() {
  const theme = useTheme();
  const lift = useKeyboardLift({ gap: 8 });
  const messages = useChatMessagesQuery(); // infinite query, pages of { items: ChatMessage[] }
  const list = useInfiniteListProps(messages);

  const renderBubble = useCallback(
    ({ element }: { element: ChatMessage }) => (
      <Chat.Bubble message={element} caption="Delivered" />
    ),
    []
  );
  // A custom renderer brings its own size spec; include the caption line.
  const getSizeSpec = useCallback(
    (m: ChatMessage) => getChatMessageSizeSpec(m, theme, { caption: true }),
    [theme]
  );

  return (
    <Animated.View style={{ flex: 1, transform: [{ translateY: lift }] }}>
      <KeyboardView style={{ flex: 1 }}>
        <Chat.List
          data={list.data}
          onStartReached={list.onStartReached}
          renderElement={renderBubble}
          getElementSizeSpec={getSizeSpec}
        />
      </KeyboardView>
      <Chat.Input onSend={(text) => sendMessage(text)} />
    </Animated.View>
  );
}
```

`ChatMessage` is `{ id, author: { id, name, avatarUrl?, avatarColor? }, isOwn, text?, images?, createdAt?, status? }`. `status` (`'sending' | 'sent' | 'delivered' | 'read' | 'failed'`) is for the reader's own messages: `'sending'` dims the bubble without changing its height, and `'failed'` adds a "Not delivered. Tap to retry." line that calls `onRetry(message)` (`Chat.Bubble`) or `onRetryMessage` (`Chat.List`). `getChatMessageSizeSpec` counts that line. `onLongPress` / `onLongPressMessage` handle long presses on a bubble. Exports: `Chat.List` / `Bubble` / `Input`, `defaultChatLabels`, `getChatMessageSizeSpec`, and the types `ChatListProps`, `ChatBubbleProps`, `ChatInputProps`, `ChatLabels`, `ChatMessageSizeSpecOptions`, `ChatAuthor`, `ChatMessage`, `ChatMessageStatus`.

### Assistant (streaming AI chat)

The list's `data` changes only twice per reply: once when the reply starts and once when it finishes. In between, tokens go to a stream store that only the streaming row subscribes to. `createTurnWriter` batches tokens into one flush every `flushMs` (default 50 ms).

```tsx
import { useListController } from 'shadowlist-utils';
import {
  Assistant,
  ASSISTANT_END_MARKER,
  createStreamStore,
  createTurnWriter,
  emptyTurn,
  type AssistantMessage,
  type AssistantReply,
} from 'shadowlist-utils/native';

export function AssistantScreen() {
  const store = useMemo(() => createStreamStore(), []);
  const list = useListController<AssistantMessage>();
  const [streaming, setStreaming] = useState(false);
  const writerRef = useRef<ReturnType<typeof createTurnWriter> | null>(null);

  // A 1pt end-marker row: "marker is viewable" means "the reader is at the bottom".
  const data = useMemo(
    () => (list.data.length ? [...list.data, ASSISTANT_END_MARKER] : []),
    [list.data]
  );

  const send = useCallback(
    (text: string) => {
      const reply: AssistantReply = {
        id: newId(),
        role: 'assistant',
        variants: [emptyTurn()],
        variantIndex: 0,
      };
      list.append([{ id: newId(), role: 'user', text }, reply]);

      const writer = createTurnWriter({
        store,
        messageId: reply.id,
        onFinish: (turn) => {
          setStreaming(false);
          // Commit the final turn to data once. Remove it from the store after that render.
          list.setData((previous) =>
            previous.map((m) =>
              m.id === reply.id && m.role === 'assistant'
                ? { ...m, variants: [turn] }
                : m
            )
          );
        },
      });
      writerRef.current = writer;
      setStreaming(true);

      streamCompletion(text, {
        onThinking: writer.appendThinking,
        onToken: writer.appendContent,
        onToolStart: (call) => writer.startToolCall(call), // returns the call id
        onToolEnd: (id, output) => writer.completeToolCall(id, output),
        onDone: (sources) => writer.complete({ sources }),
        onError: (e) => writer.fail(String(e)),
      });
    },
    [store, list]
  );

  return (
    <>
      <Assistant.List
        data={data}
        store={store}
        streaming={streaming}
        onStartReached={list.handleStartReached}
        onCopy={Clipboard.setString}
        onCopyCode={Clipboard.setString}
        onRegenerate={regenerate}
        onFeedback={sendFeedback}
        onFollowUp={send}
      />
      <Assistant.Composer
        streaming={streaming}
        onSend={send}
        onStop={() => writerRef.current?.stop()}
      />
    </>
  );
}
```

| Export                                                                                                                                                                                                                                                                                                                                                          | Description                                                                                                                                                                                                                           |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Assistant.List`                                                                                                                                                                                                                                                                                                                                                | Takes `store` (required), `streaming`, and the reply callbacks `onCopy`, `onCopyCode`, `onShare`, `onRegenerate`, `onRetry`, `onSelectVariant`, `onFeedback`, `onFollowUp`, `onOpenLink` and `onEdit`, plus `labels`.                 |
| `Assistant.Composer` (ref: `AssistantComposerHandle` with `setDraft` / `clearDraft` / `focus`)                                                                                                                                                                                                                                                                  | Has Send/Stop, attachments, a model pill, a thinking toggle and an editing banner.                                                                                                                                                    |
| `Assistant.ReplyMessage`, `UserMessage`, `Markdown`, `Thinking`, `ToolCallCard`, `AttachmentChip`, `TypingIndicator`, `Empty`, `ScrollButton`, `ActionButton`                                                                                                                                                                                                   | Building blocks for custom rows and overlays.                                                                                                                                                                                         |
| `createStreamStore()` → `AssistantStreamStore`                                                                                                                                                                                                                                                                                                                  | `subscribe` / `get` / `set` / `remove` for turns while they stream.                                                                                                                                                                   |
| `useStreamingTurn(store, messageId)`                                                                                                                                                                                                                                                                                                                            | Subscribes a custom row to its own turn.                                                                                                                                                                                              |
| `createTurnWriter({ store, messageId, flushMs?, onFinish? })` → `AssistantTurnWriter`                                                                                                                                                                                                                                                                           | Methods: `appendThinking`, `appendContent`, `startToolCall`, `completeToolCall`, `failToolCall`, `complete({ sources?, followUps? })`, `fail(error?)`, `stop()` and `isFinished`. Calls made after the turn has finished are ignored. |
| `emptyTurn()`, `ASSISTANT_END_MARKER`, `ASSISTANT_END_ID`, `defaultAssistantLabels`                                                                                                                                                                                                                                                                             |                                                                                                                                                                                                                                       |
| Types: `AssistantMessage` (`AssistantPrompt \| AssistantReply \| AssistantEndMarker`), `AssistantTurn` (streaming/done/stopped/failed), `AssistantToolCall` (running/done/failed/stopped), `AssistantSource`, `AssistantAttachment`, `AssistantFeedback`, `AssistantSuggestion`, `AssistantLabels`, `CreateTurnWriterOptions`, and `*Props` for every component |                                                                                                                                                                                                                                       |

#### Links in replies

Markdown links and source cards in assistant replies come from model output. By default `Assistant.List`, `Assistant.ReplyMessage` and `Assistant.Markdown` open only `http:`, `https:` and `mailto:` URLs, and they ignore every other scheme (`tel:`, `sms:`, app deep links and so on). To allow more schemes, or to route links through an in-app browser or an allowlist, pass `onOpenLink`:

```tsx
<Assistant.List
  onOpenLink={(url) => {
    if (isTrustedUrl(url)) Linking.openURL(url);
  }}
  ...
/>
```

When you pass `onOpenLink`, it replaces the default completely. To keep the default check for everything you don't handle yourself, fall back to the exported `openUrl`:

```tsx
import { openUrl } from 'shadowlist-utils/native';

onOpenLink={(url) => (url.startsWith('myapp://trip/') ? openTrip(url) : openUrl(url))}
```

### Other kits

Each of these is a single line of JSX. All of them accept every `ShadowList` prop.

| Kit      | Example                                                                                                                                                                                                                                                                                                                                                                          | Row type                                                                               |
| -------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| Feed     | `<Feed.List data={list.data} onEndReached={list.onEndReached} onPressItem={open} formatTime={fmt} />`                                                                                                                                                                                                                                                                            | `FeedItem { id, author, text?, images?, createdAt? }`                                  |
| Activity | `<Activity.List data={list.data} onPressItem={open} />` (sticky header/footer, separators). Also `Activity.Row` and `Activity.Header({ title, subtitle?, actions? })`.                                                                                                                                                                                                           | `ActivityItem { id, actor, action, text?, createdAt, read? }`                          |
| Nested   | `<ShadowList data={rows} renderElement={({ element }) => <Nested.Row item={element} onPressCard={open} />} />`. A row with a horizontal card list; there is no `Nested.List`.                                                                                                                                                                                                    | `NestedItem { id, title, cards: NestedCardItem[] }`                                    |
| Masonry  | `<Masonry.List data={list.data} onPressItem={open} />` (3 columns by default)                                                                                                                                                                                                                                                                                                    | `MasonryItem { id, image: { uri, width, height, alt? }, title? }`                      |
| Contacts | `<Contacts.List data={contacts} onPressItem={open} onDelete={remove} />`. `Contacts.SectionList` takes `sections` (see `groupIntoSections`). Passing `onDelete` enables swipe-to-delete.                                                                                                                                                                                         | `ContactItem { id, name, subtitle?, avatarUrl?, avatarColor? }`                        |
| Reorder  | `<Reorder.List data={favorites} onReorder={({ data }) => save(data)} />` (built on `DraggableList`)                                                                                                                                                                                                                                                                              | `ContactItem`                                                                          |
| Tree     | `<Tree.List data={tree} expandedIds={ids} onExpandedChange={setIds} onPressItem={openFile} />`                                                                                                                                                                                                                                                                                   | `TreeNode { id, name, children?, kind? }`                                              |
| Poll     | `<Poll.List poll={{ question, options, selectedId }} onVote={vote} />`. Takes `poll` instead of `data`.                                                                                                                                                                                                                                                                          | `PollOption { id, label, votes, icon? }`                                               |
| Snap     | `<Snap.List data={cards} onPressItem={open} />` (`snapToItem`)                                                                                                                                                                                                                                                                                                                   | `SnapItem { id, title?, subtitle?, image?, color? }`                                   |
| Inbox    | `<Inbox.List data={mail} onToggleRead={markRead} onToggleFlag={flag} onDelete={remove} editing={selecting} selectedKeys={keys} onSelectionChange={setKeys} />`. Native swipe actions (a full swipe runs the first; return a promise to keep the row out until it settles), a long press menu, selection while `editing`. iOS and Android swipe and menus; macOS keeps selection. | `InboxMessage { id, sender, subject, preview, receivedAt, read, flagged, avatarUrl? }` |

Each kit also exports its row or card component, a `default<Kit>Labels` object, a `<Kit>Labels` type and a `*Props` type for each component.

## Example app

The React Native example in [`templates/shadowlist-fabric-example`](../shadowlist-fabric-example) has screens built on
these kits. `TemplatesScreen.tsx` shows every control, `Grouped` and `Form` component on one page, and
`AssistantScreen.tsx` the full assistant setup. See [Example apps](../../README.md#example-apps) to run it.
