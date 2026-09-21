# ShadowListNative

A second list next to `ShadowList` whose rows are made natively. Each template is rendered by React
once; the C++ list node clones a template's shadow subtree per row, binds the row's data into the
clone's props, and commits the rows as its own children, synchronously, in the commit that needs
them. No React render per row, no JS on scroll, no per-row reconciler.

Data lives in a native store per list. Row updates, inserts, removes and per-template restyling go
straight into that store through a JSI binding and only rebuild the rows they touch.

## API

```tsx
import { ShadowListNative, type ShadowListNativeCommands } from 'shadowlist';

const ref = useRef<ShadowListNativeCommands<Post>>(null);

<ShadowListNative
  ref={ref}
  initialData={posts} // plain JSON objects; or data={posts} (controlled)
  keyExtractor={(item) => item.id} // default: item.id, else the index
  templates={{ text: <TextPost />, image: <ImagePost /> }}
  templateKey="type" // or getTemplate={(item, index) => name}
  onElementPress={({ key, index, action, elementId, item }) => {}}
  onVisibleRangeChange={({ start, end }) => {}}
  onEndReached={loadMore}
  ListHeaderComponent={<Header />}
  ListFooterComponent={<Footer />}
  ListEmptyComponent={<Empty />}
/>;

// inside a template
<ShadowListNative.View style={styles.row} action="open">
  <ShadowListNative.View
    style={styles.avatar}
    bind={{ backgroundColor: 'avatarColor' }}
  />
  <ShadowListNative.Text
    id="name"
    style={styles.name}
    bind={{ text: 'author.name' }}
  />
  <ShadowListNative.Text
    bind={{ text: '{likes} likes · {date}', color: 'likeColor' }}
  />
  <ShadowListNative.Image
    style={styles.image}
    bind={{ uri: 'images.0', hidden: '!images.0' }}
  />
  <ShadowListNative.View action="like">
    <Text>Like</Text>
  </ShadowListNative.View>
</ShadowListNative.View>;
```

### Props

| Prop                                                                                                                                                                                                                                                                                                                                            | Notes                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `initialData` / `data`                                                                                                                                                                                                                                                                                                                          | Exactly one. `initialData` (uncontrolled): seeds the store when the list mounts; later arrays are ignored and the store changes only through commands. Use it when the list owns its rows (paging with `appendItems`, local edits). `data` (controlled): each new array replaces the store (keyed diff: a row whose item is deep-equal keeps its native views), so a command's change lasts only until the next array; write edits into the source of `data`. |
| `keyExtractor`                                                                                                                                                                                                                                                                                                                                  | `(item, index) => string`. Keys must be unique; later duplicates are dropped.                                                                                                                                                                                                                                                                                                                                                                                 |
| `templates`                                                                                                                                                                                                                                                                                                                                     | `Record<name, ReactElement>`. Rendered once, hidden (`display: none`).                                                                                                                                                                                                                                                                                                                                                                                        |
| `templateKey` / `getTemplate`                                                                                                                                                                                                                                                                                                                   | Picks a row's template. Default: the template named `default`, else the first. Unknown names fall back to the default.                                                                                                                                                                                                                                                                                                                                        |
| `onElementPress`                                                                                                                                                                                                                                                                                                                                | Presses on elements with an `action`.                                                                                                                                                                                                                                                                                                                                                                                                                         |
| `onRefreshSettle`                                                                                                                                                                                                                                                                                                                               | Once per refresh, after `refreshing` turns false and the spinner has retracted (iOS: the host's settle event, 1.2 s fallback; Android: at once, the spinner floats over the rows). Apply refreshed rows here (`setData` + `scrollToStart`).                                                                                                                                                                                                                   |
| `onVisibleRangeChange`                                                                                                                                                                                                                                                                                                                          | Core visible window (includes the core's overscan). Only dispatched when set.                                                                                                                                                                                                                                                                                                                                                                                 |
| `inverted`, `followAppends`, `horizontal`, `columns`, `overscan` (viewports), `initialScrollIndex`, `stickyHeader`, `stickyFooter`, `autoHideHeader`, `autoHideFooter`, `snapToItem`, `snapToAlignment`, `refreshing`, `onRefresh`, `refreshColor`, `onStartReached`, `onEndReached`, thresholds, `onScroll`, `style`, `elementStyle`, `testID` | Same meaning as on `ShadowList`; they are the same native props.                                                                                                                                                                                                                                                                                                                                                                                              |
| `initialNumToRender` (10)                                                                                                                                                                                                                                                                                                                       | Rows mounted before the list knows its viewport.                                                                                                                                                                                                                                                                                                                                                                                                              |
| `padRows` (2)                                                                                                                                                                                                                                                                                                                                   | Extra rows mounted past the core's window each time it moves, so small scrolls rebuild nothing.                                                                                                                                                                                                                                                                                                                                                               |
| `cacheRows` (64)                                                                                                                                                                                                                                                                                                                                | Row nodes remembered after they leave the window (see "Row nodes" below).                                                                                                                                                                                                                                                                                                                                                                                     |

### Template elements

`ShadowListNative.View`, `.Text`, `.Image` are the plain components plus:

- `bind`: `{ [prop]: expression }`.
- `id`: names the element for `setTemplateStyle`.
- `action`: makes the element pressable; a press calls `onElementPress` with this action and the
  pressed row. Nested actions: the innermost wins.
- `repeat` (`ShadowListNative.View` only): the path of an array in the row. The view's children
  are one entry's template and are cloned once per entry (at most `repeatMax`); `bind` paths
  inside resolve against the entry, and `'.'` is the entry itself (an array of strings). The
  view's own `bind` still reads the row. A press inside reports the entry as `repeatIndex`.

```tsx
<ScrollView horizontal>
  <ShadowListNative.View repeat="cards" style={{ flexDirection: 'row' }}>
    <ShadowListNative.View action="card" style={styles.card}>
      <ShadowListNative.Image
        bind={{ uri: 'image.uri' }}
        style={styles.image}
      />
      <ShadowListNative.Text bind={{ text: 'title' }} />
    </ShadowListNative.View>
  </ShadowListNative.View>
</ScrollView>
// onElementPress({ key: shelfKey, action: 'card', repeatIndex: 3, item: shelf })
```

Plain React Native components (`View`, `Text`, `Image`, `ScrollView`, ...) can be used anywhere in a
template; they are cloned as they are. A horizontal `ScrollView` inside a template works (the Feed
gallery uses one).

Expressions (parsed once per template, `ShadowListNativeBinding.h`):

- `'author.name'`: the value at a dotted path; numeric segments index arrays (`'images.0.uri'`).
- `'!isRead'`: negated truthiness (null, false, 0, `''`, `[]` are falsy).
- `'{name} · {date}'`: a format string; missing values render as `''`.

Bound props:

- `text`: a `Text`'s content (the first raw text child; `ShadowListNative.Text` renders a
  placeholder so there always is one). Nested text spans are copied, not bound.
- `uri` / `source`: an `Image` source. A string becomes `[{ uri }]`; an object or array passes through.
- `hidden` / `visible`: `display: 'none'` / `'flex'`.
- Any prop ending in `Color` (and `color`): CSS strings (`#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb()`,
  `rgba()`, `transparent`, a few names) are converted to ARGB; numbers pass through.
- Anything else: the raw value is parsed as that prop (`opacity`, `width`, `aspectRatio`, ...).
  Values are the item's JSON values; they are not run through React Native's JS style processing.

### Commands (`ref`)

| Command                                                                  | Notes                                                                                                                                                                                                                  |
| ------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `updateItem(key, patch)`                                                 | Shallow merge. Only that row is rebuilt (its families are kept, so the platform sees an update, not a remount). Returns false for an unknown key.                                                                      |
| `replaceItem(key, item)`                                                 | Replace the item.                                                                                                                                                                                                      |
| `insertItems(index, items)`, `appendItems(items)`, `prependItems(items)` | Returns the new count. Keys already present are skipped. MVCP keeps the visible rows in place.                                                                                                                         |
| `removeItems(keys)`, `moveItem(key, toIndex)`                            |                                                                                                                                                                                                                        |
| `setData(items)`                                                         | Same as a new `data` array.                                                                                                                                                                                            |
| `setTemplateStyle(template, elementId, style \| null)`                   | Merges `style` over element `id` of `template` in every row, now and later. `null` clears it. Rebinds that template's rows in place.                                                                                   |
| `getItem(key)`, `getKeys()`, `getCount()`                                | Synchronous reads of the native store.                                                                                                                                                                                 |
| `scrollToIndex(index, viewPosition)`, `scrollToEnd()`                    | Run after every mutation made before them is laid out, so `appendItems(...)` then `scrollToEnd()` lands on the new row (see "Scroll commands").                                                                        |
| `scrollToStart()`                                                        | Offset 0 with the header in view, sequenced like `scrollToEnd` (after every earlier mutation is laid out). `setData` then `scrollToStart` shows refreshed rows from the top instead of MVCP keeping the old first row. |
| `scrollToOffset`, `setStartReachedEnabled`, `setEndReachedEnabled`       | ShadowList's view commands.                                                                                                                                                                                            |

All mutations are synchronous in JS and request one coalesced commit; the rows change in that
commit (typically the next frame).

## Architecture

ShadowListNative is not a new native component. It renders the existing `ShadowListView` with one
extra prop, `nativeListId`, and a hidden templates container. Everything the host views do (scroll,
offset echo, MVCP corrections, sticky header/footer, snap, refresh, concealment) is shared as is:
the rows it synthesizes are ordinary `ShadowListElementView` children with an `elementKey`, exactly
what ShadowList's React rows are, so `ShadowListViewShadowNode::layout` measures and places them
with the same code.

```
JS  <ShadowListNative>                         C++
    ├─ open + first setData (render, pre-commit) ──▶  ShadowListNativeEngine (per listId, registry)
    │                                             rows_ (key, folly::dynamic item, template, version)
    ├─ <ShadowListView nativeListId=…>            keys_ snapshot (for the core)
    │   ├─ header template                        compiled templates, row nodes, tag -> key
    │   ├─ <ShadowListTemplateView templateType="native" display:none>
    │   │    └─ <ShadowListElementView nativeID="shadowlist-template:NAME">  (one per template)
    │   │         └─ template elements (nativeID="shadowlist:{i,b,a}")
    │   ├─ …synthesized rows (ShadowListElementView clones)…
    │   └─ footer template
    └─ ref.updateItem(…) ─ JSI ─▶ engine mutates store ─▶ state nudge ─▶ commit
```

### Files

- `cpp/.../ShadowListViewSpec/ShadowListNativeEngine.{h,cpp}`: store, template compiler, row
  synthesis/rebinding, window reconcile, layout coverage check, registry.
- `cpp/.../ShadowListViewSpec/ShadowListNativeBinding.h`: expression and color parsing (folly-free,
  unit-tested in `packages/shadowlist-core-tests/tests_native_binding.cpp`).
- `cpp/.../ShadowListViewSpec/ShadowListNativeJSI.{h,cpp}`: `globalThis.__shadowListNative`.
- `cpp/.../ShadowListViewSpec/ShadowListViewComponentDescriptor.h`: `adopt` (store keys to the core),
  `cloneShadowNode` and `appendChild` overrides (rows into the tree).
- `cpp/.../ShadowListViewSpec/ShadowListViewShadowNode.{h,cpp}`: engine/keys carried across clones,
  `didLayout` after placement.
- `src/ShadowListNative.tsx`, `src/native/binding.ts`, `src/types.ts` (`ShadowListNative*` types),
  `src/ShadowListViewNativeComponent.ts` (`nativeListId` prop).
- Examples (`packages/shadowlist-fabric-example/src`): `FeedNativeScreen.tsx` (route `FeedNative`:
  `initialData`, pages appended, likes/hides kept as local edits, refresh applied on settle),
  `ChatNativeScreen.tsx` (route `ChatNative`: inverted, six bubble templates, send / status via
  `updateItem` / retry / delete / load earlier), `NestedNativeScreen.tsx` (route `NestedNative`:
  shelves with repeated cards, a horizontal `snapToItem` deals list in the header, a two-column grid
  mode with a sticky range readout and an empty state).

### Templates

Template metadata travels in `nativeID`, the one string prop every host component forwards to C++
(`bind` or `id` on a host component would be dropped by React Native's view config). The engine
compiles a template when the container's children change: it walks the subtree, parses each
`shadowlist:{json}` marker (`i` = id, `b` = bindings, `a` = has action) and records a signature
(props pointers + child counts). A layout clone of the container has the same signature and is not
recompiled. A changed template bumps its version; if its shape (component names, child counts and instance handles)
is unchanged, existing rows are rebound in place, otherwise rebuilt.

Base props per element are the prototype's props with the marker stripped (so clones can be
flattened like normal views) plus any `setTemplateStyle` override. Elements with an `action` keep
their marker so they are never flattened away (a flattened element would pass its touches to an
ancestor).

### Rows

A row is `buildNode(templateRoot, item)`, recursively:

- props: the element's base props, or `cloneProps(base, RawProps(patch))` where `patch` holds the
  bound values; a bound `text` goes onto the element's first `RawText` child.
- new row: every node gets a new family (`descriptor.createFamily`) with a fresh tag, the
  prototype's instance handle, `createInitialState`, `createShadowNode`. Tags are even and start at
  2^30 (React counts even tags up from 2; Android routes odd tags to the legacy renderer).
- rebind (same shape): `existing->clone({props, children})` per node, keeping families, so
  Fabric emits updates, images keep their loaded state, and nothing remounts. Unchanged subtrees
  are returned as is.

A clone must reuse the prototype's instance handle (never null): a clone with none crashes on its
first event. Events from clones reach the template element's fiber, which is also what makes
presses work (below).

### Row nodes and the event-target rule

`rowNodes_` remembers, per key, the node last committed (updated from `didLayout` after placement)
and the row/template versions it was built from. A mounted row whose versions match is reused as is.

A row that was _unmounted_ is never reused: unmounting drops its families' event emitters to a
retain count of 0, which releases their event targets permanently (`EventEmitter::setEnabled`), so a
remounted clone would silently drop touches and image events. `hasLiveEventTarget` checks this; such
a row is rebuilt with new families. The cache still matters when React re-renders the list and hands
its children back without the rows (the `appendChild` path): those rows are still mounted and are
reused.

### Getting rows into the tree

Rows are children no React component renders. They enter in two places:

1. `ShadowListViewComponentDescriptor::cloneShadowNode`: after the normal clone + `adopt()` (which
   ran `Virtualizer::update` with the store's key snapshot), any clone carrying props, children or
   state reconciles rows against the core's window. If they differ, a second node is constructed from
   the first with `ShadowNodeFragment{.children}` and React's runtime reference is transferred to it.
   The second construction is deliberate: a node built with a children fragment has not configured
   its Yoga subtree, so the layout pass configures the new rows (point scale factor, errata). Editing
   the adopted node's children in place would leave fresh rows laid out with Yoga defaults. Layout-only
   clones (empty fragment) are left alone.
2. `appendChild`: React building a list (a new node, or `cloneNodeWithNewChildren` + appends). When the
   templates container is appended, the rows are appended right after it, before the footer, so
   z-order matches ShadowList (header below rows, footer/sticky overlay above).

Once rows are children, Yoga lays them out (`position: absolute`, full width) in the same pass, and
`ShadowListViewShadowNode::layout` feeds their sizes to the core and places them. Measurement is
synchronous: a new row is measured in the commit that mounts it.

### Window

`reconcileRows` mounts the core's measurement window (`getVisibleIndices`: the viewport plus
`overscan` viewports each side). While the mounted rows are contiguous and still cover it, nothing is
rebuilt; when the window runs past an end it remounts `window ± padRows`. Before the core has a
viewport it seeds `initialNumToRender` rows from the start, the end (inverted) or `initialScrollIndex`.
Rows are always taken from the key snapshot the core reconciled in the same commit, so a row is never
mounted for a key the core does not know (it would be unplaced).

On device the Feed screen reconciles about once per 60-100 scroll frames, building 2-4 rows each time
(`[SL] native:` trace lines).

### Commit-free scrolling

The host's scroll report is a state update, i.e. a commit: `adopt` pushes the offset into the core,
and the clone path reconciles rows in that same commit. No JS runs on scroll.
`ShadowListViewShadowNode::layout` calls `didLayout`, which requests one more commit when the measured
rows no longer cover the viewport by half a viewport (rows smaller than estimated), deduplicated by
geometry so an unfixable gap cannot loop.

### Data path and JSI

`globalThis.__shadowListNative` (installed by the descriptor through the `RuntimeScheduler`, on
first use of `ShadowListView`) has `setData`, `insertItems`, `updateItem`, `removeItems`,
`moveItem`, `scrollToIndex`, `setTemplateStyle`, `configure`, `getItem`, `getKeys`, `getCount`, `resolveTag`,
`open`, `close`. Items cross as `folly::dynamic` (`jsi::dynamicFromValue`). Mutations on a list with no
live engine are no-ops.

Why JSI rather than view commands with JSON strings: a command reaches the host view on the UI
thread; on Android that is Java with no way back to the C++ store except a state update, and state
updates for one node coalesce (`EventQueue::enqueueStateUpdate` keeps only the last), which would
lose all but the last mutation of a frame. With JSI, JS writes the store directly and the state
update that follows (`requestCommit`) is only a nudge, so coalescing is harmless. It also gives
synchronous reads and needs no host code on either platform.

`requestCommit` calls `state->updateState(callback)` on the list's newest state with a copy of the
committed data and `containerOffsetEnabled_ = false`, so a nudge never re-applies an old offset
correction.

Render and commit: until a list node has used the engine (`committed`), nobody else can observe its
store, so render opens the engine and seeds it (`configure`, `setData`); the list node is then created
with its rows and the first frame has them. After that the store is only written from the commit
phase: a new controlled `data` array is applied in a layout effect (a nudge commit, the rows the old
data showed stay until then), `configure` changes likewise, and commands run from event handlers. A
render React throws away (a transition, Suspense, StrictMode's double render) only touched an engine
that nothing else holds. If the binding is not installed yet (the descriptor is created lazily, on
first use), the component renders an empty `ShadowListView` to get it built, then polls per frame and
re-renders. This costs one or two frames on the very first list of the app.

Lifetime: the registry is weak. An engine lives while a list node holds it or JS holds its handle:
`open(listId)` returns a JSI host object that owns the engine, created lazily in render and closed in
the unmount cleanup of a layout effect (StrictMode's simulated unmount re-opens it). A handle from a
discarded render is garbage; every handle dies with its runtime, so a JS reload cannot pin engines.
List ids carry a per-runtime (per module evaluation) nonce, `shadowlist-native-<nonce>-<n>`, so a new
runtime, whose React tags restart too, never reaches an engine (and its `prototypeRawProps_`, keyed by
tag) left from the old one. `open` sweeps dead registry entries.

### Scroll commands

`scrollToIndex` and `scrollToEnd` do not go to the host view. A view command reaches the host on
the UI thread and becomes a state update of its own, which races the data: it can commit before
the rows it targets exist, in the same commit before they are measured (a newest message that
just grew its failed line stays under the composer), or, queued right after a data nudge for the
same node, the event queue keeps only one of the two. Instead the engine records the command with
the store version at the time of the call (`requestScroll`); `didLayout` requests a commit once
that version is reconciled and laid out, and that commit's `adopt` hands it to the core directly
(`Container::scrollToEnd` / `scrollToIndex`) before `Virtualizer::update`. The host's own command
bookkeeping (`_commandSequence`, carried on every scroll report) is not touched, so a report can
never replay an engine scroll. `[SL] native: scroll index=... after=...` traces each one.

Like the host commands, an engine scroll stops momentum. The host commands stop the fling on the
UI thread when they are issued. An engine scroll reaches the core in a commit, so the host stops
the fling when it mounts the correction:

- The engine runs that frame as idle. `adopt` clears `userScrolled`/`scrollPhase` for the frame,
  so the core does not read the fling as a gesture driving the correction.
- It records the core operation's id (`setMomentumYieldToken`).
- The layout pass stamps that token on the published state (`momentumYieldToken_`), along with
  every retarget of it.
- A host mounting a correction whose `commitToken_` matches it stops the deceleration, the
  scroll-to-top animation, the Android snap glide and settle poll. It then writes the offset
  instead of shifting it onto the coasting view.

A finger on the list keeps its drag, and the core lets the drag cancel the command. Momentum
reports that arrive before the mount do not cancel the command. `scrollToStart` is its own
operation type (`ScrollToStart`), so momentum does not move its target the way it moves an MVCP
anchor.

The nudge itself holds the list's state strongly: the layout pass replaces the node's state
object (`setStateData`) after `adopt` attached it, so a weak reference expired on every commit that
published geometry and all later nudges were dropped until something else committed the list.

### Presses

Touches on a clone are dispatched to the clone's event emitter, whose target is the template
element's fiber, so React's responder system runs on the template element. `ShadowListNative.View`
with an `action` becomes the responder and, on release, resolves the touch to a row with
`resolveTag(listId, changedTouches[0].target)`: each touch carries the hit view's own tag (the
event's top-level `target` is the template's). The engine maps every synthesized tag to its row key.

A release more than 10pt from where the touch began is dropped (`PRESS_SLOP`, like Pressability's
slop): a quick flick the scroll view never claims, e.g. at a scroll edge, otherwise ends as a
release on the template responder and fires the action (seen on Android in Chat (Native)).

No pressed-state feedback: the template's React state is shared by all rows.

The press context is kept on `globalThis` (`__shadowListNativeContext`). A Fast Refresh of
`ShadowListNative.tsx` re-evaluates the module; with a module-level context, template elements
remounted with the new module's code read a new context while the list still provided the old one,
and every press was silently dropped (rows rebuilt, `built=N`, toolbar fine). A production template
remount (e.g. a `key` change on a template) rebuilds the rows with the new instance handles and
presses keep routing (verified on iOS).

### Threading

JS mutates the store on the JS thread; commits read it on the committing thread. One mutex per
engine; lock order is `Container::coreMutex` then the engine mutex (the JS thread never takes the core
lock).

## Supported / not supported

Supported and verified on iOS (simulator, see below): templates with View/Text/Image/ScrollView,
bindings (text, uri, colors, hidden, generic props), multiple templates per list, header/footer,
`autoHideHeader`, pull to refresh, `onEndReached` paging, `scrollToIndex`, `updateItem` (one row
rebound), insert/prepend/remove with the visible area kept, `setTemplateStyle` (rows rebound in place),
presses routed to the right row including rows rebuilt after scrolling away.

Also verified (Chat (Native), Explore (Native)): `inverted` (opens at the newest message, prepend
of a history page keeps the visible rows to the pixel, appends below keep the visible area,
`scrollToEnd` after append/grow), `onStartReached`, `horizontal` (a list in a list header, prepend
and append of cards), `columns` (two-column grid, remove reflows), `moveItem` (visible rows stay,
MVCP), `repeat` (a 2x2 image grid, cards in a horizontal ScrollView; growing the array rebinds in
place), a theme switch rebinding every template (presses still route afterwards).

Exercised in Explore (Native): the grid is the one controlled list (`data`). Its likes, `✕`
removes, Shuffle and Clear / Undo clear each replace the array, and the store follows it. Clear
passes `[]`, which shows `ListEmptyComponent`. Undo clear passes the grid's cards back as they
were: likes kept, removed cards still gone. The grid has a `stickyHeader` whose readout
combines `onVisibleRangeChange` (the core window with overscan) with the array's length:
"21–42 of 197". The deals list uses `snapToItem`. Feed (Native): `initialData` + paging
by `appendItems`, local edits that survive paging/refresh, refresh via `onRefreshSettle` + `setData`

- `scrollToStart`, theme colors as template styles (a theme switch rebinds, data untouched).

Dev lifecycle, on iOS and Android: row presses route correctly after a Fast Refresh of
`ShadowListNative.tsx`, and rows and presses are correct after three Metro reloads in a row.

Wired but not exercised on device: sticky footer, `followAppends`.

### Nesting

Rows are clones, so a nested list cannot be a React child of a row. Two ways work, both native:

1. **Repeat inside a native scroller** (Explore shelves): the shelf template holds a horizontal
   `ScrollView`, and its cards are a `repeat` over the shelf's `cards` array. The cards are cloned
   with the shelf, in the same commit, from the shelf's own data; tapping a card reports the shelf
   and `repeatIndex`; `updateItem(shelf, { cards })` rebinds the shelf in place. Not virtualized
   horizontally, so keep the arrays bounded (`repeatMax`). Horizontal position: a rebind keeps the
   `ScrollView` family, so it keeps its offset; a shelf that left the window is rebuilt (new
   families, see "Row nodes"), and comes back at offset 0, like a regular `ShadowList` shelf that
   is not in `persistentKeys`. Recycled platform scroll views do not carry offsets across shelves
   (checked on iOS).
2. **A ShadowListNative outside the rows** (Explore deals): a React-rendered list in the header,
   footer or beside the list is an ordinary `ShadowListNative` with its own store and commands. It
   is not recycled, so it keeps its position.

A `ShadowListNative` cloned per row (one engine per row, bound to a row's array) is not
supported: every clone would carry a copy of the nested templates, needs its own store keyed by
row, and a rebuilt row would lose the nested core's offset and measurements. `repeat` covers the
shelf case without any of that.

Not supported:

- Per-row React state, hooks, effects or callbacks inside templates (a template renders once).
- Pressed-state feedback, `Pressable`/`onPress` inside templates (use `action`).
- Binding inside nested text spans; conditional structure (use `hidden`, or another template).
- Sticky section headers (`stickyHeaderIndices` + overlay), drag to reorder, viewability callbacks,
  `getElementSizeSpec` predictions, `trackElementSizes`. Refresh deferral is explicit: hold the new rows
  until `onRefreshSettle` (Feed (Native) does).
- Style values that need React Native's JS processing beyond colors (e.g. `transform` strings) in
  bound values or `setTemplateStyle`.

## Known gaps / risks

1. Repeat entries keep their nodes by position, not by key: inserting at the front of a repeated
   array rebinds every entry after it (images re-load their new sources).
2. `scrollToOffset` is still a host command, so it can coalesce with a data nudge queued in the
   same frame (the offset is lost, the data is not).
3. `hasLiveEventTarget` reads the emitter's event target, which the mount path also writes; both run
   during commit/mount, but it is not formally synchronized.
4. One transient text glitch (overlapping lines in one row) was seen once in a mid-scroll screenshot
   and could not be reproduced in 24 further mid-scroll captures.
5. Android: a template prop change that React sends in two commits the engine does not both see
   (it compiles templates in each commit that clones the list, so this should not happen) would
   leave the accumulated raw props with the older value of a prop only the skipped diff changed.
   See "Android" below.

6. `ListEmptyComponent` (shared with `ShadowList`, pre-existing): the core's total size excludes the
   empty template, so on Android it is clipped (nothing shows) and on iOS it shows but is outside the
   content bounds, so it is not hit-testable (no buttons in it; Explore puts Undo clear in its toolbar).
7. Fast Refresh does not hot-swap the template element components (old code keeps rendering until a
   reload); presses keep working either way.

Fixed since the first version: a template element whose fiber React replaces (same host shape)
now rebuilds its rows, because the instance handle is part of the template's shape; before, rows
rebound in place kept the dead handle and dropped their touches.

## Build and test

```sh
# iOS (new codegen prop / new files need pod install)
cd packages/shadowlist-fabric-example/ios
RCT_NEW_ARCH_ENABLED=1 bundle exec pod install --no-repo-update
xcodebuild -workspace ShadowListExample.xcworkspace -scheme ShadowListExample \
  -configuration Debug -sdk iphonesimulator -destination id=<udid> \
  -derivedDataPath <dd> build
xcrun simctl install <udid> <dd>/Build/Products/Debug-iphonesimulator/ShadowListExample.app
xcrun simctl launch --console-pty --terminate-running-process <udid> shadowlist.example -SLRoute FeedNative
```

If the link fails with `Sealable::Sealable()` undefined, the shared Pods hold the Release React
prebuilt while `.last_build_configuration` claims Debug (after a Release build plus `pod install`):
`printf Release > Pods/React-Core-prebuilt/.last_build_configuration` and build again; the prebuilt
swap phase then restores Debug.

```sh
# Android (debug compiles the canonical packages/shadowlist-core directly)
cd packages/shadowlist-fabric-example/android && ./gradlew app:installDebug -PreactNativeArchitectures=arm64-v8a
# Unit tests
cd packages/shadowlist-core-tests/build && cmake .. && make && ./shadowlist_core_tests
cd packages/shadowlist-fabric && npx jest
```

Device trace: `[SL] native: rows=[low..high] mounted=N built=B rebound=R` per reconcile that changed
the rows (debug builds).

## Android

Verified on the `Medium_Phone_API_35` emulator (debug, bridgeless), driven with `adb shell input`
and screenshots, trace via `adb logcat -s SL`:

- Feed (Native): styling matches Feed (avatars, rounded image frames, text attributes, separator,
  gallery `ScrollView`), images load, fast flings down (paging 20 -> 80 rows) and back to the top
  leave no blank rows after settling, `♡` rebinds only that row (`built=0 rebound=1`), `Hide`
  removes the pressed row, Prepend / Remove top keep the visible rows in place, Update, Accent
  names (`rebound=N`), presses on rows rebuilt after scrolling away and back, a theme switch
  (rows built afterwards keep their full style, presses still route).
- Chat (Native): opens at the newest message (inverted), press -> alert -> Delete removes that
  message, send appends + `scrollToEnd` + status `updateItem` (`Sent`), incoming appends keep the
  visible area, header "load earlier" prepends, flings through the history, repeat image grids.
- Explore (Native): shelves with repeated cards in a horizontal `ScrollView`, `+ Card`
  (`updateItem` of the array), card like (`repeatIndex`), `↑ Top` (`moveItem`, visible rows stay),
  the horizontal deals list in the header (press toggles `Picked`), Grid mode (`columns: 2`,
  controlled `data`): `✕` removes and reflows, likes, Shuffle, and Clear / Undo clear
  (`98->0->98`, likes kept), each a new array.
- No red box, no soft exceptions from the synthesized rows (the template element's
  `setIsJSResponder` on a never-mounted tag logs nothing), no crash.

There is no Android-specific host code: ShadowListNative uses the existing `ShadowListView` /
`ShadowListViewManager` / `ShadowListElementView`, and the engine, JSI binding and descriptor are
shared C++ compiled by `android/shadowlist/jni/CMakeLists.txt` (it globs
`cpp/react/renderer/components/ShadowListViewSpec/*.cpp`). `ShadowListViewManager.setNativeListId`
is a no-op setter. What differs, all in the shared engine under `#ifdef RN_SERIALIZABLE_STATE` /
`__ANDROID__`:

1. **Raw props.** Android mounts a view from `Props::rawProps`, not from the C++ props object, and
   (with `enableAccumulatedUpdatesInRawPropsAndroid` off, the default) a props object's `rawProps`
   is only what built it: all props for a node React created, only React's diff for a node React
   updated. So:
   - `cloneWithPatch` merges the base's `rawProps` with the bound patch before parsing, so a row
     carries the base's props plus its bindings.
   - The engine keeps each template element's full raw props (`prototypeRawProps_`, keyed by the
     prototype's tag), merging every new props object's diff into it at compile time, and bases
     the element's `baseProps` on that. Without it, rows built after a template update (a theme
     switch) got only the diff: image frames lost `borderRadius`/`overflow`. Rebound rows were
     fine either way (an update only needs the diff).
2. **Colors.** Java reads color props with `getInt`, so bound colors are passed as signed 32-bit
   ARGB (as `processColor` does on Android). An unsigned value above `INT_MAX` saturated to
   `0x7FFFFFFF` (translucent white): bound avatar backgrounds rendered grey.
3. **Hidden templates.** `useTraitHiddenOnAndroid` is off, so the `display: none` templates
   container is mounted as a zero-size view. It is invisible, and the Java host never reads template
   types (header/footer placement is decided in C++); nothing to do.
4. **Tags.** Synthesized tags are even (`FIRST_NATIVE_TAG = 1 << 30`, step 2) because
   `ViewUtil.getUIManagerType` routes odd tags to the legacy UIManager. Touches carry the clone's
   tag in `changedTouches[0].target`, as on iOS; `resolveTag` finds the row.
5. **State nudge / JSI.** Nudged states do not re-apply offsets (prepend/remove keep the visible
   rows); `__shadowListNative` installs through the `RuntimeScheduler` on bridgeless.
6. **Row concealment** is disabled on Android (`CONCEAL_UNSETTLED_ROWS`).
7. **Glyphs.** Android draws some symbols with emoji presentation (a bare U+2665 is a red emoji that
   ignores the bound `color`); append U+FE0E (text presentation), as the examples' hearts do.

Dev trap: the JS component and the native engine must come from the same tree. With a JS change
to the binding protocol loaded by Metro over an older native build, presses silently stopped
routing; rebuild the app after pulling engine or binding changes.
