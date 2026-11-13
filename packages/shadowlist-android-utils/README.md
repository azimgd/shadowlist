# ShadowListUtils for Android

ShadowListUtils is the templates package for ShadowListKit on Android: themed lists, rows and primitives for feeds,
chats, inboxes, activity, polls, contacts, grids and more. It is written in Kotlin on ShadowListKit and the shared C++ core.

## Install

Maven Central:

```kotlin
dependencies {
  implementation("io.github.azimgd:shadowlist-utils:0.10.0")
}
```

JitPack, with `maven("https://jitpack.io")` in the repositories:

```kotlin
dependencies {
  implementation("com.github.azimgd.shadowlist:shadowlist-utils:v0.10.0")
}
```

Both bring [ShadowListKit](../shadowlist-android/README.md) with them.

### Requirements

| Dependency | Version |
| --- | --- |
| Android | API 24 or newer |
| Java | 17 or newer |

The AAR ships `libshadowlistutils.so` with the C++ runtime linked statically, aligned for 16 KB pages. It holds the
template logic the platforms share. The list core stays in the kit's `libshadowlistkit.so`.
`./gradlew :ShadowListUtils:publishToMavenLocal` installs the current checkout as `io.github.azimgd:shadowlist-utils`
with `VERSION_NAME` from `ShadowListUtils/gradle.properties`.

The default image loader fetches over the network. An app that shows remote images declares the `INTERNET`
permission, or replaces the loader.

## Usage

Every type is in `com.shadowlist.utils`.

```kotlin
class FeedScreen(context: Context, posts: List<ShadowListUtilsFeedItem>) {
  val list = ShadowListUtilsFeedList(context).also {
    it.onPressItem = { post -> open(post) }
    it.onEndReached = { loadMore() }
    it.items = posts
  }
}
```

Add `list` to your screen's view hierarchy. A single-list template is a `ShadowListKitListView`, and every property of the
kit list stays reachable: `headerView`, `footerView`, `refreshing`, `scrollToItem` and the rest. It is its own data
source and delegate and gives closures instead. An app that needs a delegate call the template does not surface
uses a `ShadowListKitListView` with the library's rows.

## API

Lengths are dp as `Float`, like the theme's tokens, where the kit takes pixels. A length in pixels says so with a
`Px` suffix. Colors are ARGB `Int`s.

Text follows the reader's text size setting, sizes and line heights alike. An avatar's initials and a reorder tile's
label keep their size. At a text scale of 1.3 or more a masonry or tile title takes up to four lines, and a contact's
name and subtitle wrap.

A row mirrors its frames when the app lays out from right to left. The kit list mirrors its own parts, like swipe
actions, by the view's layout direction. An app lays out from right to left with `android:supportsRtl="true"` and a
right-to-left locale.

`ShadowListUtilsCore` is the bridge to the shared C++ core. It is an implementation detail: it is not part of the
documented API and can change in any release.

Item-based template lists have these five members, with their own item type. Poll uses `poll` and `setPoll`
instead of `items` and `setItems`:

| API | Notes |
| --- | --- |
| `items` | The items shown. Setting it is `setItems(items, false)`. |
| `setItems(items, animated)` | Shows new items. The list diffs them by `key` and configures the cells whose item changed. |
| `onPressItem` | A tap on an item. While it is null the list has `allowsSelection` off. |
| `rowForItem` | A row of the app's own for every item, in place of the template's. |
| `labels` | The template's strings. |

### Theming

```kotlin
val theme = ShadowListUtilsTheme.DARK.let { it.copy(colors = it.colors.copy(accent = 0xFFFF7A00.toInt())) }
val list = ShadowListUtilsFeedList(context, theme)
```

A list made without a theme follows the device appearance. A list made with a theme, or given one through `theme`
later, keeps it. A row holds the theme it was built with. There is no global current theme.

| API | Notes |
| --- | --- |
| `ShadowListUtilsTheme` | `colors`, `typography`, `spacing`, `radius`, `grouped`, `rowInset`, `tapTarget`. A data class: `copy` changes a token. |
| `ShadowListUtilsTheme.LIGHT`, `DARK`, `system(context)` | The included themes, and the one of the two the device appearance asks for. |
| `ShadowListUtilsTheme.HAIRLINE_PX` | The thickness of a hairline, one device pixel. |
| `ShadowListUtilsThemeColors` | `background`, `elevated`, `elevated2`, `groupedBackground`, `groupedCell`, `label`, `secondaryLabel`, `tertiaryLabel`, `separator`, `fill`, `accent`, `accentSoft`, `onAccent`, `blue`, `green`, `orange`, `red`, `redSoft`, `avatarPalette`. |
| `ShadowListUtilsThemeTypography` | `largeTitle`, `title2`, `title3`, `headline`, `body`, `callout`, `subhead`, `footnote`, `caption`, each a `ShadowListUtilsTextStyle`. |
| `ShadowListUtilsTextStyle` | `fontSize`, `fontWeight`, `lineHeight`, `letterSpacing`, `with(fontWeight)`, `layout(text, widthPx, maxLines)`. Every line of a layout is the style's line height tall, at the reader's text size. |
| `ShadowListUtilsFontWeight` | `REGULAR`, `SEMIBOLD`, `BOLD`: weights 400, 600 and 700 of the system font. Before API 28 semibold is the system's medium face. |
| `ShadowListUtilsThemeSpacing`, `ShadowListUtilsThemeRadius` | `xxs`, `xs`, `sm`, `md`, `lg`, `xl`, `xxl`, and `sm`, `md`, `lg`, `xl`, `pill`. |
| `ShadowListUtilsThemeGrouped` | `inset`, `radius`, `rowInset`, `rowHeight`, `captionCase`, `chevron`. `ShadowListUtilsCaptionCase.NONE` keeps the title of a section header as written. |

### Labels

All visible and accessibility strings come from a template's `labels`, a data class whose defaults are the English
strings. Some labels are functions.

```kotlin
list.labels = ShadowListUtilsChatLabels(failed = "Nicht zugestellt. Tippen zum Wiederholen.")
```

| API | Notes |
| --- | --- |
| `ShadowListUtilsRelativeTimeLabels` | `now`, `minutes`, `hours`, `days`. `format(dateMs, nowMs)` words a time with them, and a week or more back as the month and day in the reader's locale. |
| `ShadowListUtilsMasonryLabels`, `ShadowListUtilsFeedLabels`, `ShadowListUtilsReorderLabels`, `ShadowListUtilsSnapLabels`, `ShadowListUtilsContactsLabels`, `ShadowListUtilsInboxLabels`, `ShadowListUtilsChatLabels` | One per template. |

### Rows

A row is the model of what one cell shows. Its layout is a pure function of the row and a width, safe off the UI
thread, and its view only copies the frames the layout holds. The template lists build their rows from items. A
`ShadowListUtilsList` takes rows directly, of the library's or of the app's own.

| API | Notes |
| --- | --- |
| `ShadowListUtilsRow` | `key`, `contentVersion`, `reuseIdentifier`, `makeView(context)`, `layout(widthPx)`. A new `contentVersion` under the same key configures the cell again. In a horizontal list `widthPx` is the list's height, and the layout's `widthPx` is the row's size along the list. |
| `ShadowListUtilsRowLayout` | `widthPx`, `heightPx`. Subclasses add their row's frames. A layout never changes once made. |
| `ShadowListUtilsRowView` | An open `ShadowListKitListCell`. `row`, `configure(row, layout)`, and `fill(row)` and `apply(row, layout)` to override. Without a layout it lays itself out on the UI thread. |
| `ShadowListUtilsLayoutCache` | `layout(row, widthPx)`, `prefetch(rows, widthPx, around)`, `invalidate(key)`, `invalidateAll()`. `layout` is safe on any thread. |
| `ShadowListUtilsList` | `theme`, `rows`, `setRows(rows, animated)`, `onEndReached`, `onStartReached`, `onRefresh`, `onScroll`. Setting `onRefresh` turns pull to refresh on. Set the kit's `refreshing` back to false when done. |

The layout of each of the library's rows holds a frame for every part of the row, in pixels, and the typeset text of
its text parts. A frame of a part the row lacks is empty.

| Layout | Members |
| --- | --- |
| `ShadowListUtilsSectionHeaderLayout` | `title`, `titleFrame`, `count`, `countFrame`. |
| `ShadowListUtilsMasonryCardLayout` | `imageFrame`, `title`, `titleFrame`. |
| `ShadowListUtilsFeedRowLayout` | `avatarFrame`, `name`, `nameFrame`, `handle`, `handleFrame`, `time`, `timeFrame`, `text`, `textFrame`, `imageFrame`, `stripFrame`, `stripImageFrames`, `separatorFrame`. `stripImageFrames` are in the strip's content. |
| `ShadowListUtilsReorderRowLayout` | `avatarFrame`, `name`, `nameFrame`, `subtitle`, `subtitleFrame`, `gripFrame`, `separatorFrame`. |
| `ShadowListUtilsReorderTileLayout` | `swatchFrame`, `label`, `labelFrame`, `title`, `titleFrame`. |
| `ShadowListUtilsSnapCardLayout` | `cardFrame`, `captionFrame`, `title`, `titleFrame`, `subtitle`, `subtitleFrame`. `captionFrame` is in the card, and the text frames are in the caption. |
| `ShadowListUtilsNestedRowLayout` | `title`, `titleFrame`, `listFrame`. |
| `ShadowListUtilsNestedCardLayout` | `imageFrame`, `title`, `titleFrame`. |
| `ShadowListUtilsContactRowLayout` | `avatarFrame`, `name`, `nameFrame`, `subtitle`, `subtitleFrame`, `chevronFrame`, `separatorFrame`. |
| `ShadowListUtilsInboxRowLayout` | `unreadDotFrame`, `checkFrame`, `avatarFrame`, `sender`, `senderFrame`, `flagFrame`, `time`, `timeFrame`, `subject`, `subjectFrame`, `preview`, `previewFrame`. |
| `ShadowListUtilsChatBubbleLayout` | `maximumBubbleWidthPx`, `avatarFrame`, `name`, `nameFrame`, `bubbleFrame`, `text`, `textFrame`, `caption`, `captionFrame`, `failed`, `failedFrame`, `imagesFrame`, `imageFrames`. `imageFrames` are in `imagesFrame`. |

### Primitives

| API | Notes |
| --- | --- |
| `ShadowListUtilsAvatar(context, size, theme)` | `show(name, imageUrl, color)`. Initials on a color derived from the name, with the image on top once it loads. |
| `ShadowListUtilsImageView(context)` | `url`, `cornerRadius`, `placeholderColor`, `image`. Fills its frame and clips to it. |
| `ShadowListUtilsImageLoader` | An open class with `load(url, into)` and `cancel(view)`. `ShadowListUtilsImageLoader.shared` is the loader every image view uses. The default keeps decoded images in memory, fetches each url once, ends a fetch nobody waits for and keeps responses on disk in the platform's HTTP cache. Replace it with a subclass to use the app's image stack. |
| `ShadowListUtilsListHeader(context, title, subtitle, theme)`, `ShadowListUtilsListFooter(context, text, theme)` | Header and footer blocks for a list's `headerView` and `footerView`. |
| `ShadowListUtilsSectionHeader(key, title, count, theme)` | The row of a section header, with `ShadowListUtilsSectionHeaderLayout` and `ShadowListUtilsSectionHeaderView`. |
| `ShadowListUtilsItemSeparator(context, theme)` | An inset hairline. `inset` defaults to the theme's `rowInset`. |
| `ShadowListUtilsSpinner(context, size, color, theme)` | Centered activity indicator. |
| `ShadowListUtilsRedactedText.bars(layout, lineHeightPx)` | One bar per line of a text layout that holds text, as wide as the line's text, for a placeholder row. |
| `ShadowListUtilsEmptyState(context, title, message, icon, theme)` | What a screen says before it has anything to show. |

Every primitive view has a settable `theme`.

### Masonry

| API | Notes |
| --- | --- |
| `ShadowListUtilsMasonryList` | A grid of image cards. Sets `numberOfColumns` to 3. |
| `ShadowListUtilsMasonryItem(key, image, title)`, `ShadowListUtilsMasonryImage(url, width, height, alt)` | Only the ratio of `width` and `height` is read. |
| `ShadowListUtilsMasonryCard(item, theme, labels)` | The row, with `ShadowListUtilsMasonryCardLayout` and `ShadowListUtilsMasonryCardView`. |

### Feed

| API | Notes |
| --- | --- |
| `ShadowListUtilsFeedList` | A feed of posts. `onPressImage(item, index)` and `formatTime(createdAtMs)` besides the shared members. Every image is an accessibility element with its `alt`, and a button while `onPressImage` is set. |
| `ShadowListUtilsFeedItem(key, author, text, images, createdAtMs, placeholder)`, `ShadowListUtilsFeedAuthor`, `ShadowListUtilsFeedImage` | A `placeholder` item draws its strings as bars and its images as colored frames. |
| `ShadowListUtilsFeedRow(item, theme, labels, formatTime)` | The row, with `ShadowListUtilsFeedRowLayout` and `ShadowListUtilsFeedRowView`. `time` is the post's age in words, worked out when the row is made. One image keeps its aspect between 3:4 and 16:9, and several scroll sideways in a strip. |

### Reorder

| API | Notes |
| --- | --- |
| `ShadowListUtilsReorderList` | Contacts that reorder on a touch and hold. Sets `reorderEnabled`. Items are `ShadowListUtilsContactItem`s. |
| `ShadowListUtilsReorderGrid` | Tiles that reorder, in 3 columns. `tileAspectRatio` shapes every tile alike. |
| `onMoveItem(sourceIndex, destinationIndex, items)` | On both. The list already shows the moved items. A screen reader moves an item by one place through the same call. A drop where the item was picked up does not run it. |
| `ShadowListUtilsReorderTileItem(key, title, label, color, aspectRatio)` | The item of a tile. |
| `ShadowListUtilsReorderRow(item, theme, labels)`, `ShadowListUtilsReorderTile(item, aspectRatio, theme, labels)` | The rows, with their layouts and views. |

### Snap

| API | Notes |
| --- | --- |
| `ShadowListUtilsSnapList` | Cards the list rests on. Sets `snapToItem`. `cardHeight` in dp, a quarter of the height of the list's window when null. |
| `ShadowListUtilsSnapItem(key, title, subtitle, imageUrl, imageAlt, color)` | The item of a card. |
| `ShadowListUtilsSnapCard(item, height, theme, labels)` | The row, with `ShadowListUtilsSnapCardLayout` and `ShadowListUtilsSnapCardView`. |

### Nested

There is no list for nested rows. Put them into a `ShadowListUtilsList` with `setRows`.

| API | Notes |
| --- | --- |
| `ShadowListUtilsNestedRow(item, theme, onPressCard)` | A title over a horizontal list of cards. The horizontal list keeps its place while its row is off screen. |
| `ShadowListUtilsNestedCard(item, theme)` | A card of that list. |
| `ShadowListUtilsNestedItem(key, title, cards)`, `ShadowListUtilsNestedCardItem(key, title, imageUrl, imageAlt)` | The items of a row and of a card. |

### Contacts

| API | Notes |
| --- | --- |
| `ShadowListUtilsContactsList` | A list of contacts. `onDelete(item)` adds a trailing swipe action in the theme's red, and `disclosureIndicator` shows a chevron on rows that can be pressed. |
| `ShadowListUtilsContactsSectionList` | The same in sections under sticky headers. `sections`, `setSections(sections, animated)`, `sectionIndexTitles`. Setting `items` groups them. |
| `ShadowListUtilsContactItem(key, name, subtitle, avatarUrl, avatarColor)`, `ShadowListUtilsContactSection(key, title, items)` | The item and a section of them. |
| `ShadowListUtilsContactRow(item, showsChevron, theme)` | The row, with `ShadowListUtilsContactRowLayout` and `ShadowListUtilsContactRowView`. |
| `shadowListUtilsGroupIntoSections(items)` | Sections by the first character of each name in upper case, sorted in the reader's locale, with `#` last for names that start with no letter. |

### Inbox

| API | Notes |
| --- | --- |
| `ShadowListUtilsInboxList` | A mail list with swipe actions, a menu on a hold and selection while the kit's `editing` is on. Sets `showsSeparators`. |
| `onToggleRead`, `onToggleFlag`, `onDelete`, `onReply` | Each takes the message. An action whose closure is null is left out of the swipes and the menu. |
| `selectedKeys`, `onSelectionChange(keys)` | The selection while editing. Leaving editing clears it and reports no keys. The menu's Select reports its one key, for the app to turn editing on with. |
| `ShadowListUtilsInboxMessage(key, sender, subject, preview, receivedAtMs, read, flagged, avatarUrl)` | The item. Only `avatarUrl` has a default. |
| `ShadowListUtilsInboxRow(item, editing, theme, labels)` | The row, with `ShadowListUtilsInboxRowLayout` and `ShadowListUtilsInboxRowView`. `time` is the message's age in words, worked out when the row is made. |

### Chat

| API | Notes |
| --- | --- |
| `ShadowListUtilsChatList` | Messages oldest first. Sets `inverted`. `onRetryMessage`, `onLongPressMessage`, `captionForItem` and `showsAuthor` besides the shared members. A hold on a bubble runs `onLongPressMessage`, and the lift that ends it is not a tap. A press on or near the line under a failed message runs `onRetryMessage`. |
| `ShadowListUtilsChatMessage(key, author, isOwn, text, images, createdAtMs, status)`, `ShadowListUtilsChatAuthor`, `ShadowListUtilsChatMessageStatus` | The status of the reader's own messages is `SENDING`, `SENT`, `DELIVERED`, `READ` or `FAILED`. A failed message adds a retry line. |
| `ShadowListUtilsChatBubble(item, caption, showsAuthor, theme, labels)` | The row, with `ShadowListUtilsChatBubbleLayout` and `ShadowListUtilsChatBubbleView`. `showsAuthor` false drops the name and avatar of a message from someone else and keeps the avatar's place. |

### Activity

`ShadowListUtilsActivityList` shows `ShadowListUtilsActivityItem(key, actor, action, text, createdAtMs, read)`.
The actor is `ShadowListUtilsActivityActor(name, avatarUrl, avatarColor)`. Times are milliseconds since 1970;
`read` defaults to true. `formatTime` overrides the relative-time labels. A press requests an action and never
changes read state automatically.

```kotlin
val list = ShadowListUtilsActivityList(context)
list.items = activities
list.onPressItem = { item -> markRead(item.key) }
```

### Poll

`ShadowListUtilsPollList` takes one controlled `ShadowListUtilsPollData(question, options, selectedKey)`.
Each `ShadowListUtilsPollOption(key, label, votes, icon)` has stable identity and a Double vote count. Shared C++ helpers
calculate totals, leading option and percentages. The app applies the vote and sets the new model; the list never
changes votes or selection on its own. With no `onPressItem`, rows are noninteractive.

```kotlin
val list = ShadowListUtilsPollList(context)
list.poll = poll
list.onPressItem = { option -> submitVote(option.key) }
// After the application updates its model:
list.setPoll(updatedPoll, animated = true)
```

`labels` customizes option accessibility text and total-vote text. An optional native image `icon` replaces the first Unicode scalar of the option label in its chip;
leading options use the accent color and selected options an accent border. `rowForItem` supports custom rows.
Custom `headerView` and `footerView`, including null, survive model and label updates. Activity and Poll rows expose
matching layout/view types and follow large text and right-to-left layout like the other templates.

### Activity header and chat input

`ShadowListUtilsActivityHeader` shows a title, optional subtitle and wrapping actions. Each
`ShadowListUtilsActivityHeaderAction` has `label`, `onPress`, optional native image `icon` and optional
`accessibilityLabel`. Set the view as the activity list's `headerView`; the example's Mark all read action updates
application-owned items.

`ShadowListUtilsChatInput` is the exported multiline composer used by the Chat example. Its initializer accepts
`defaultValue` for an uncontrolled draft. A non-null `value` makes the application own the text;
`onChangeText` requests edits. `onSend` receives trimmed text. Sending clears only an uncontrolled draft and ignores
whitespace or `disabled` input. `maxLength` defaults to 500 UTF-16 units and never accepts half a surrogate pair.
`labels.placeholder` and `labels.send` supply the visible/accessibility strings. The containing screen owns keyboard
avoidance; UIKit's example places the input against its keyboard layout guide.

## Tests

`./gradlew :ShadowListUtils:connectedDebugAndroidTest` runs the library's tests on a connected device or emulator.
They load the native libraries and replay the vector files of `packages/shadowlist-core-tests/vectors`.

## Example app

[shadowlist-android-example](../../templates/shadowlist-android-example) has a screen per template. Open one with
`--es SLRoute` and a route: `Feed`, `Masonry`, `Chat`, `Contacts`, `SectionList`, `Reorder`, `ReorderGrid`, `Snap`,
`Nested`, `Inbox`, `Activity` or `Poll`. `--es SLStates 1` adds placeholder posts to Feed and captions and sending and failed messages to
Chat, `--es SLSelectDemo 1` opens Inbox editing with two rows selected, and `--es SLImages 0` loads no image.


## Tree

`ShadowListUtilsTreeList` takes `items` of `ShadowListUtilsTreeNode(key, name, children, kind)`.
`children == null` is a leaf; an empty list can still show a folder glyph. The shared kit's `TreeSource` owns
visible traversal, expansion splices and retained expansion intent. Kotlin only encodes the application's node objects.

`initialExpandedKeys` supplies uncontrolled initial state. Leave `expandedKeys` null for internal expansion, or set it
to a set for app-controlled expansion: `onExpandedChange` requests a set and the app assigns its accepted set.
Hidden branches and keys whose nodes arrive later retain expansion intent. `onPressItem` handles leaves.
`scrollToNode(key, viewPosition)` uses a visible stable key and leaves a collapsed descendant alone.
`rowForItem` receives `ShadowListUtilsTreeListRenderItemInfo` with the item, depth, indent, `isExpanded`,
`hasChildren` and `toggle`. `indentWidth` and `labels.expand`/`labels.collapse` customize the default row.

## Board

`ShadowListUtilsBoardList<Item>` takes generic `ShadowListUtilsBoardColumn<Item>` values (`key`, `title`, `items`),
a `keyExtractor` and a `rowForItem` renderer. Each board isolates its drag identity, and its columns retain their
native lists across updates. `ShadowListUtilsBoardCard` is an optional row with title, label, detail and color.

`onMoveItem` requests a `ShadowListUtilsBoardMoveInfo<Item>` containing the item, `sourceColumnKey`,
`destinationColumnKey`, `destinationKey` and `destinationIndex`. Assign the new columns in that callback to accept
it; leaving state unchanged rejects it. `shadowListUtilsMoveBoardItem` applies the shared C++ move plan to application
objects. A destination key wins over the fallback index, and an empty key appends. Both within-column reorders and
cross-column drops use this callback. The kit owns drag flights, foreign insertion sessions and cleanup.

`renderColumnHeader` and `renderColumnFooter` stay outside draggable rows; a renderer returning null removes its view.
`columnWidth`, `sidePadding`, `columnHeight` (`FILL` or `CONTENT`), `minColumnHeight`, `maxColumnHeight` and
`estimatedItemHeight` use the shared column layout helpers and take dp. Content mode gives all column bodies one
bounded height. `labels` customizes column summaries and empty-column messages. The example app includes Tree and
Board routes. The board container hosts a horizontal kit list with column snapping and drag-driven sideways edge
scrolling. Its column views stay mounted while their inner lists virtualize rows. Cross-list accessibility actions
append to the named destination and use the same application-controlled move callback.


## Assistant

`ShadowListUtilsAssistantList` renders application-owned `items` and live `turns` keyed by message key.
Use `setItems` and `setTurns` for animated updates. Saved reply variants, selection, feedback and terminal status
stay in the application model; action callbacks request changes. A live turn overrides the selected saved variant.
The `.endMarker` message remains a one-dp row excluded from anchoring. The list defaults to inverted layout,
following appends and dismissing the keyboard on drag. `rowForItem` replaces a default transcript row.

`ShadowListUtilsAssistantReplyMessage` renders shared Markdown parser output, interleaved tool calls, thinking,
sources and follow-ups. Copy, share, regenerate, retry, variant selection, feedback, follow-up and link callbacks
are available on the list and standalone reply. Thinking and tool disclosure stays keyed to the message across
streaming updates. Row layouts capture immutable model and presentation snapshots before background measurement.

`ShadowListUtilsAssistantComposer` reuses the multiline chat input. `value` and `onChangeText` support controlled
drafts; otherwise it owns the draft. `onSend` receives trimmed text and application-owned attachments and requests
a cleared draft. `maxLength` defaults to 4000 UTF-16 units. Streaming leaves draft editing enabled and presents Stop
unless `sendWhileStreaming` permits sending the current draft. Attachment removal, model choice, thinking and edit
cancellation are controlled callbacks. `setDraft`, `getDraft`, `clearDraft` and `focus` support application workflows.
The Assistant example route demonstrates send, incremental live turns, finish, failure, retry, stop and editing.


### Standalone Assistant and Poll components

The transcript and standalone `ShadowListUtilsAssistantThinking` and
`ShadowListUtilsAssistantToolCallCard` share disclosure layout and rendering. Thinking takes
`thinking`, `thinkingMs` and `active`; the collapsed preview keeps the last 160 UTF-16 units.
Tool cards take the existing typed `call`, plus `grouped` and `separated`. Both own only local
expansion state. Expanded text uses native selection with zero text insets; application turns
and calls remain unchanged. Existing `labels` supply the text and status formatters.

`ShadowListUtilsAssistantEmpty` takes `suggestions` of `ShadowListUtilsAssistantSuggestion(title, prompt)`
and `onSelectSuggestion`; selection returns the unchanged suggestion. Place it over the transcript,
not among its rows. `ShadowListUtilsAssistantScrollButton` takes `visible` and `onPress`; hiding it
preserves its slot while removing touch and accessibility interaction. Its fade lasts 180 ms.
`ShadowListUtilsAssistantTypingIndicator` presents one localized progress element with three
composited dots, a 600 ms pulse and 160 ms stagger; animations stop when hidden or detached and
respect the system animation preference.

`ShadowListUtilsPollFooter(totalVotes, theme)` is the exported total-vote footer used by PollList's
default footer. Its existing Poll `labels.totalVotes` formatter receives the application total.
Custom or explicitly cleared PollList footers remain application-owned.

The native `Components` example route uses all six views: suggestions update the screen's selected
prompt, disclosures expand, Finish changes a running tool to done and increments the displayed vote
total, and the scroll button returns to the first component. The composition and controlled-field
examples below use the same route; these additions do not imply complete visual/API parity.

### Form and Grouped composition

`ShadowListUtilsFormCard(context, children)` inserts an inset hairline between native child views.
`ShadowListUtilsFormValue(context, text)` retains the supplied value and displays an em dash when
empty; `accessibilityLabel` overrides the displayed value. `ShadowListUtilsFormButton(context, label)`
provides a centered `onPress` action and `destructive` styling. The application owns the
resulting state. An unwired native button is disabled.

`ShadowListUtilsGroupedCard(context, children)` applies themed inset and rounded card styling without
inserting separators. Compose `ShadowListUtilsGroupedSeparator(context, inset)` explicitly.
`ShadowListUtilsGroupedCaption(context, title)` uses theme caption casing and heading accessibility.
`ShadowListUtilsGroupedSection(context, title, children)` composes a caption and card; null or an empty
title hides the caption. Supplying the same child views retains their identity and order.
The Components route demonstrates Save/Remove callbacks updating a read-only account value.

```kotlin
val value = ShadowListUtilsFormValue(context, "3 saved routes")
val save = ShadowListUtilsFormButton(context, "Save route")
save.onPress = { value.text = "4 saved routes" }
val form = ShadowListUtilsFormCard(context, listOf(save))
val section = ShadowListUtilsGroupedSection(context, "Account", listOf(value))
```

Grouped.Row/Actions and the remaining Controls are separate parity milestones.

### Controlled form and text fields

`ShadowListUtilsFormRow(context, label, children)` aligns its controls with the trailing value
edge; `stacked` or the existing large-text threshold places the label above its controls.
`ShadowListUtilsFormInput(context, value, accessibilityLabel)` takes a controlled `value` and
`onChange`. Numeric inputs default to `DECIMAL_PAD` (decimal, without a signed flag); an explicit
`keyboardType` wins. Numeric and email inputs default to no capitalization/correction, with
optional `autoCapitalize` and `autoCorrect` overrides. `returnKeyType` supports the RN names
`DONE`, `NEXT`, `GO`, `SEARCH`, and `SEND`; `onSubmitEditing` receives the native submit action.
The application must apply accepted edits:

```kotlin
val name = ShadowListUtilsFormInput(context, "Lisbon", "Route name")
name.onChange = { name.value = it }
val row = ShadowListUtilsFormRow(context, "Route", listOf(name))
```

`ShadowListUtilsTextField(context, value)` has `onChange`, `placeholder`, `keyboardType`,
`accessibilityLabel`, and `multiline` (false by default). The multiline box uses native monospace
14sp text and is 104dp high; the single-line box is 40dp. The accessible label defaults to the
placeholder. Correction and capitalization are disabled. `NUMERIC` is signed and decimal,
`DECIMAL_PAD` is decimal only, and `NUMBER_PAD` is integer only; iOS-only keyboard choices fall
back to native text as in RN's Android adapter.

`ShadowListUtilsSearchField(context, value)` takes `onChange`, an optional native `icon`, and
`ShadowListUtilsSearchFieldLabels.placeholder` (default `Search`), also its accessible label.
Android follows RN's platform behavior for the iOS-only clearButtonMode affordance.
All four share ChatInput's native editing owner: programmatic values are silent, user edits
request `onChange`, and app-controlled replacements wait until native composition finishes.
The new wrappers do not add an uncontrolled mode or a secure-entry prop. Components owns the
controlled Route, Budget, Search and Notes values.

### Action controls

`ShadowListUtilsPillButton(context, label)` exposes `variant` (`PRIMARY`, `TINTED`, `PLAIN`, or
`DESTRUCTIVE`), `disabled` (false by default), and `onPress`. It is 40dp high with a minimum width
of 72dp. `ShadowListUtilsDashedButton(context, label)` supplies an add action with `disabled` and
`onPress`; its wrapped label grows with native large text.

`ShadowListUtilsIconButton(context, icon, accessibilityLabel)` is a 40dp round action with
`onPress`. The caller's child keeps its accessibility/interaction flags and may own a nested
button action. Standard native touch delegation expands the outer action by 4dp without widening
its layout or accessibility bounds. Expanded hits go to the outer native control, not the nested
child. Delegates coexist with siblings and an existing parent delegate and are removed on detach.
Set decorative flags on the glyph you provide when it is purely visual.
`ShadowListUtilsIndexBadge(context, index)` displays the integer unchanged in a 22dp tinted square.

These three RN action callbacks are required. Native property-based wiring allows an initially
unwired instance, disabled until its callback is set; this is a native initialization extension,
not an RN optional callback. Disabled Pill/Dashed controls retain their styling at 0.4 opacity.

`theme.pressFeedback` defaults to `HIGHLIGHT`. `RIPPLE` uses Android's standard RippleDrawable in
`colors.fill` for PillButton and IconButton; UIKit falls back to highlight. DashedButton and
FormButton retain their existing highlight behavior. One shared helper uses the native pressed
state; no separate gesture or animation state machine is introduced.

Components now uses matching light-theme data: Continue disables itself, Add route increments the
badge and re-enables Continue, and More routes reports its application callback. The standalone
scroll button is centered at its intended 36dp size.

Android expanded hit targets preserve existing parent delegates and each control’s native accessibility bounds. On API 29 and newer, delegate metadata refers to the mounted control; an opaque existing parent delegate retains its own metadata. Earlier API levels keep native touch delegation without the newer metadata/hover APIs.

### Chips and wrapping choices

`ShadowListUtilsChip(context, label)` has independent optional `onPress` and `onClear`
actions. An absent press callback keeps normal styling and button accessibility; it does not
disable clear. Selection uses the inherited Android `isSelected` / `setSelected` state
(the counterpart of Swift `selected`), defaults to false and updates paint/accessibility
without invoking either callback. `variant` is `TINTED` (default) or `PLAIN`, `size` is
`REGULAR` (default, 34dp) or `COMPACT` (28dp), and `mono` defaults to false. Labels stay on
one line and intrinsic width is capped at 240dp. `maxFontSizeMultiplier` caps only the label;
null or a value below 1 leaves native text scaling uncapped. `accessibilityLabel` defaults
to the label and `clearAccessibilityLabel` to `Clear <label>`. Clear is a separate accessible
button with an 8dp expanded hit area, bounded by its parent.

`ShadowListUtilsChoiceChip(context, label, tint, selected)` uses the supplied color, a selected
ring and the existing 14dp check icon. Its inherited `isSelected` remains controlled; `onPress`
requests an application update. No extra disabled, clear or variant props are introduced.
`ShadowListUtilsChipRow(context, children)` measures native children and passes their sizes to
the shared C++ `wrapLayout`, using `theme.spacing.sm` between children and lines and the existing
native RTL frame mirroring. Child identity and order remain application-owned.

The Components route demonstrates toggling and removing Direct routes, clearing a compact
monospaced seat preference without a primary press handler, and controlled Train/Flight choices.
