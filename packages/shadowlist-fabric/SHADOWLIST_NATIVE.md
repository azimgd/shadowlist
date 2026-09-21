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
  data={posts} // plain JSON objects
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

| Prop                                                                                                                                                                                                                                                                                                                           | Notes                                                                                                                                                                                                                             |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `data`                                                                                                                                                                                                                                                                                                                         | The rows. A new array replaces the store, diffed by key and item: a row whose item is deep-equal keeps its native views. Imperative commands change the store without touching `data`; they hold until the next new `data` array. |
| `keyExtractor`                                                                                                                                                                                                                                                                                                                 | `(item, index) => string`. Keys must be unique; later duplicates are dropped.                                                                                                                                                     |
| `templates`                                                                                                                                                                                                                                                                                                                    | `Record<name, ReactElement>`. Rendered once, hidden (`display: none`).                                                                                                                                                            |
| `templateKey` / `getTemplate`                                                                                                                                                                                                                                                                                                  | Picks a row's template. Default: the template named `default`, else the first. Unknown names fall back to the default.                                                                                                            |
| `onElementPress`                                                                                                                                                                                                                                                                                                               | Presses on elements with an `action`.                                                                                                                                                                                             |
| `onVisibleRangeChange`                                                                                                                                                                                                                                                                                                         | Core visible window (includes the core's overscan). Only dispatched when set.                                                                                                                                                     |
| `inverted`, `horizontal`, `columns`, `overscan` (viewports), `initialScrollIndex`, `stickyHeader`, `stickyFooter`, `autoHideHeader`, `autoHideFooter`, `snapToItem`, `snapToAlignment`, `refreshing`, `onRefresh`, `refreshColor`, `onStartReached`, `onEndReached`, thresholds, `onScroll`, `style`, `elementStyle`, `testID` | Same meaning as on `ShadowList`; they are the same native props.                                                                                                                                                                  |
| `initialNumToRender` (10)                                                                                                                                                                                                                                                                                                      | Rows mounted before the list knows its viewport.                                                                                                                                                                                  |
| `padRows` (2)                                                                                                                                                                                                                                                                                                                  | Extra rows mounted past the core's window each time it moves, so small scrolls rebuild nothing.                                                                                                                                   |
| `cacheRows` (64)                                                                                                                                                                                                                                                                                                               | Row nodes remembered after they leave the window (see "Row nodes" below).                                                                                                                                                         |

### Template elements

`ShadowListNative.View`, `.Text`, `.Image` are the plain components plus:

- `bind`: `{ [prop]: expression }`.
- `id`: names the element for `setTemplateStyle`.
- `action`: makes the element pressable; a press calls `onElementPress` with this action and the
  pressed row. Nested actions: the innermost wins.

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

| Command                                                                                            | Notes                                                                                                                                             |
| -------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| `updateItem(key, patch)`                                                                           | Shallow merge. Only that row is rebuilt (its families are kept, so the platform sees an update, not a remount). Returns false for an unknown key. |
| `replaceItem(key, item)`                                                                           | Replace the item.                                                                                                                                 |
| `insertItems(index, items)`, `appendItems(items)`, `prependItems(items)`                           | Returns the new count. Keys already present are skipped. MVCP keeps the visible rows in place.                                                    |
| `removeItems(keys)`, `moveItem(key, toIndex)`                                                      |                                                                                                                                                   |
| `setData(items)`                                                                                   | Same as a new `data` array.                                                                                                                       |
| `setTemplateStyle(template, elementId, style \| null)`                                             | Merges `style` over element `id` of `template` in every row, now and later. `null` clears it. Rebinds that template's rows in place.              |
| `getItem(key)`, `getKeys()`, `getCount()`                                                          | Synchronous reads of the native store.                                                                                                            |
| `scrollToIndex`, `scrollToOffset`, `scrollToEnd`, `setStartReachedEnabled`, `setEndReachedEnabled` | ShadowList's view commands.                                                                                                                       |

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
    ├─ render: binding.setData(listId, …)  ──▶  ShadowListNativeEngine (per listId, registry)
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
- Example: `packages/shadowlist-fabric-example/src/FeedNativeScreen.tsx` (drawer route `FeedNative`).

### Templates

Template metadata travels in `nativeID`, the one string prop every host component forwards to C++
(`bind` or `id` on a host component would be dropped by React Native's view config). The engine
compiles a template when the container's children change: it walks the subtree, parses each
`shadowlist:{json}` marker (`i` = id, `b` = bindings, `a` = has action) and records a signature
(props pointers + child counts). A layout clone of the container has the same signature and is not
recompiled. A changed template bumps its version; if its shape (component names and child counts)
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
`moveItem`, `setTemplateStyle`, `configure`, `getItem`, `getKeys`, `getCount`, `resolveTag`,
`retain`, `release`. Items cross as `folly::dynamic` (`jsi::dynamicFromValue`).

Why JSI rather than view commands with JSON strings: a command reaches the host view on the UI
thread; on Android that is Java with no way back to the C++ store except a state update, and state
updates for one node coalesce (`EventQueue::enqueueStateUpdate` keeps only the last), which would
lose all but the last mutation of a frame. With JSI, JS writes the store directly and the state
update that follows (`requestCommit`) is only a nudge, so coalescing is harmless. It also gives
synchronous reads and needs no host code on either platform.

`requestCommit` calls `state->updateState(callback)` on the list's newest state with a copy of the
committed data and `containerOffsetEnabled_ = false`, so a nudge never re-applies an old offset
correction.

The component calls `setData` during render (idempotent; the store diffs), so the list node is
created with its data and the first frame has rows. If the binding is not installed yet (the
descriptor is created lazily, on first use), the component renders an empty `ShadowListView` to get
it built, then polls per frame and re-renders. This costs one or two frames on the very first list
of the app.

Lifetime: the registry holds an engine strongly while JS pins it (first data call / `retain` on
mount, until `release` on unmount) and weakly otherwise; list nodes hold it strongly. StrictMode's
unmount/remount re-pins the same engine.

### Presses

Touches on a clone are dispatched to the clone's event emitter, whose target is the template
element's fiber, so React's responder system runs on the template element. `ShadowListNative.View`
with an `action` becomes the responder and, on release, resolves the touch to a row with
`resolveTag(listId, changedTouches[0].target)`: each touch carries the hit view's own tag (the
event's top-level `target` is the template's). The engine maps every synthesized tag to its row key.

No pressed-state feedback: the template's React state is shared by all rows.

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

Wired but not exercised on device: `inverted`, `horizontal`, `columns`, `snapToItem`, sticky
header/footer, `moveItem`, `ListEmptyComponent`, `onVisibleRangeChange`.

Not supported:

- Per-row React state, hooks, effects or callbacks inside templates (a template renders once).
- Pressed-state feedback, `Pressable`/`onPress` inside templates (use `action`).
- Binding inside nested text spans; conditional structure (use `hidden`, or another template).
- Sticky section headers (`stickyHeaderIndices` + overlay), drag to reorder, viewability callbacks,
  `getElementSizeSpec` predictions, `trackElementSizes`, refresh data deferral (`useRefreshDefer`).
- Style values that need React Native's JS processing beyond colors (e.g. `transform` strings) in
  bound values or `setTemplateStyle`.

## Known gaps / risks

1. Template changes rebind rows with the old families when the shape is unchanged. If React replaced
   a template element's fiber (different component type at the same position, same host shape), the
   rebound rows keep the old instance handle and their events are dropped until they are rebuilt.
2. `hasLiveEventTarget` reads the emitter's event target, which the mount path also writes; both run
   during commit/mount, but it is not formally synchronized.
3. One transient text glitch (overlapping lines in one row) was seen once in a mid-scroll screenshot
   and could not be reproduced in 24 further mid-scroll captures.
4. Android host behavior is untested (it compiles).

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
# Android (compiles; host untested)
cd packages/shadowlist-fabric-example/android && ./gradlew app:assembleDebug -PreactNativeArchitectures=arm64-v8a
# Unit tests
cd packages/shadowlist-core-tests/build && cmake .. && make && ./shadowlist_core_tests
cd packages/shadowlist-fabric && npx jest
```

Device trace: `[SL] native: rows=[low..high] mounted=N built=B rebound=R` per reconcile that changed
the rows (debug builds).

## Porting notes for Android

There is no Android host work in the usual sense: ShadowListNative uses the existing
`ShadowListView` / `ShadowListViewManager` / `ShadowListElementView`, and the engine, JSI binding
and descriptor are shared C++ already compiled by `android/shadowlist/jni/CMakeLists.txt` (it globs
`cpp/react/renderer/components/ShadowListViewSpec/*.cpp`). `ShadowListViewManager.setNativeListId`
is a no-op setter. `./gradlew app:assembleDebug` builds. What needs verifying and likely fixing, in
order:

1. **Clone props on Android.** Android mounts views from `Props::rawProps` (`RN_SERIALIZABLE_STATE`),
   not from the C++ props object. `cloneWithPatch` in `ShadowListNativeEngine.cpp` merges the base's
   `rawProps` with the bound patch before parsing, but the base itself (a template prototype's props)
   only holds React's _last update_ unless `enableAccumulatedUpdatesInRawPropsAndroid` is on. Check
   that a new row's platform view receives the full style (background, padding, text attributes).
   If not: accumulate the prototype's raw props yourself (keep a merged `folly::dynamic` per
   template element across template updates, and pass it as the base), or enable the flag. Rows
   look like unstyled text if this is wrong.
2. **Hidden templates.** iOS skips mounting the `display: none` container (`Trait::Hidden`); on
   Android that is gated by `ReactNativeFeatureFlags::useTraitHiddenOnAndroid()`. If it is off, the
   templates mount as zero-size views; verify they are invisible and that `ShadowListView.java`'s
   child handling does not treat the `templateType="native"` view as header/footer (it should ignore
   unknown types, check `addView`/sticky code paths).
3. **Tags.** Synthesized tags are even (`FIRST_NATIVE_TAG = 1 << 30`, step 2) because
   `ViewUtil.getUIManagerType` routes odd tags to the legacy UIManager. Verify events and
   `resolveTag` with real touches.
4. **Presses and the JS responder.** React calls `setIsJSResponder` on the template element's tag,
   which is never mounted (hidden). iOS ignores the missing view; Android's `SurfaceMountingManager`
   may log a soft exception ("view not found"). Harmless if only logged; if it throws in debug,
   guard it or make template action elements `pointerEvents` passthrough. Also check that
   `nativeEvent.changedTouches[0].target` is the touched clone's tag on Android (it is on iOS).
5. **State nudge.** `requestCommit` uses `ConcreteState::updateState(callback)`; on Android the
   state then goes to Java through `getDynamic()`/the partial-update constructor. The nudge only
   copies data, so no new fields are involved, but confirm the Java view does not re-apply an
   offset from a nudged state (`containerOffsetEnabled` is false in it).
6. **Row concealment** is disabled on Android already (`CONCEAL_UNSETTLED_ROWS`), nothing to do.
7. **JSI install.** Uses `RuntimeScheduler` from the ContextContainer (as the trace does); the JS
   side polls until `__shadowListNative` appears. Confirm it appears on Android bridgeless.
8. **Test plan** (mirror of the iOS one): route `FeedNative`, fast and slow fling top/bottom, no blank
   rows after settling; tap `♡` (only that row changes; `[SL] native: … built=0 rebound=1` in
   `adb logcat -s SL`), `Hide` (that row goes), toolbar Prepend/Remove top (visible rows stay put),
   Update, Accent names (all names recolor, `rebound=N`), taps after scrolling away and back.
