# Extracting the native list engine into shadowlist-core/host

Status: done on 2026-10-07. `SLKEngine` is now `ListDriver` in `packages/shadowlist-core/host` and drives both kits. This note keeps the original analysis. Its file and line references point at code that no longer exists, and it keeps the `SLK` prefix the kits used then. The kits now use `ShadowListKit` (`SLKListView` is `ShadowListKitListView`).

Abbreviations used for file references:

- iOS = packages/shadowlist-uikit/Sources/ShadowListKit/SLKListView.mm
- ENG = packages/shadowlist-android/ShadowListKit/src/main/cpp/SLKEngine.{hpp,cpp} (line numbers are for the .cpp unless marked .hpp)
- JNI = .../cpp/SLKCoreJNI.cpp
- KT = .../java/com/shadowlist/kit/SLKListView.kt, DRG = SLKDragController.kt, GST = SLKScrollGesture.kt

## Verdict

Yes, do it. SLKEngine already has no platform types, and about 80% of it is the same as SLKListView.mm, line for line. Move it to `host/ListDriver.{hpp,cpp}` (class `ListDriver`). Both builds pick it up with no build changes: Android CMake globs `host/*.cpp` through sources.cmake, and the iOS Example/project.yml includes `host/*.cpp`. iOS then keeps only UIKit work: cells, the reuse pool, transforms, gestures and the delegate proxy. Expected size: SLKListView.mm goes from 1415 to about 1000 lines (about -400). SLKEngine.cpp/.hpp (607 lines) moves into host. Repo-wide that is about 400 fewer lines, and there is one implementation to fix instead of two.

Suggested names follow host/ conventions (no prefix, noun-style files): `ListDriver`, `ListSettings`, `PassInput`, `PassResult`, `RowRect`, `MountPlan`, `ScrollLanding`. Drop the `SLK` prefix and the `kit` namespace when moving.

---

## Frame loop

1. **runFrames pass loop + clamp + echo token** (highest value)
   - iOS 606-684 (FrameInput fill 612-639, loop 640-670, clamp 653-656). ENG 94-179 (`makeFrameInput`, `runFrames`, `clampOffset`).
   - API: `PassResult ListDriver::runPasses(const PassInput&)`, the current `SLKEngine::runFrames` unchanged. iOS fills `PassInput` from `coreOffset`/insets, then applies `contentSize` before `writeCoreOffset(result.offset)`.
   - The clamps match: iOS `maxCoreOffset` (287-293) = content + trailing + leading - bounds = content - windowAlong. That is ENG `clampOffset` exactly.
   - Lines removed: iOS about -75.
   - Risk: iOS writes contentOffset and contentSize **between** passes (649, 661) and re-reads `coreOffset` each pass (611). ENG uses a local `offset` and only the final value is written. The math is the same, but iOS stops flushing `contentSize` mid-loop. Keep the order "contentSize, then contentOffset" when applying the result.

2. **Settle detection + settling-frame cap**
   - iOS 665-683 (the predicate is copied twice, and the cap is a bare literal `60`). ENG 150, 159-160, 181-185 (`SLK_MAX_SETTLING_FRAMES`). The same predicate also exists in core: Container.cpp:301-304 (`computeOffsetBand`).
   - API: `bool Container::hasPendingCommand() const` (operation, pendingScrollToEnd/Start, scrollToIndexTarget). Use it in `computeOffsetBand`, `ListDriver` and the Fabric shadow node. `PassResult.settling` already returns the capped value.
   - Lines removed: iOS about -12, core about -3.
   - **Possible bug (iOS)**: iOS reschedules with `dispatch_async(main){ setNeedsLayout }` on every settling pass, with no dedupe flag (680). That layout can run in the same frame, before any new scroll report arrives. The 60-"frame" cap therefore counts layout passes, not frames, and can run out within a few ms. Android dedupes (`settleScheduled`, KT 540-548) and waits a real frame (`postOnAnimation`). After the extraction iOS should use one flag plus a CADisplayLink or a next-runloop hop, the way Android does.

3. **Reached callbacks**
   - iOS 230-245 (`installCoreCallbacks` with a weak self) + 985-999. ENG 21-24, 161-164. KT 514-515 + 783-792.
   - API: already in `PassResult.reachedStart/End`. iOS reads the flags instead of installing callbacks.
   - Lines removed: iOS about -18. No behavior difference.

4. **Layout-pass prologue: geometry read, band check, user-scroll tracking**
   - iOS 528-557 (inline in `layoutSubviews`). KT 421-461 (`readGeometry`, `trackUserScroll`, `inBand`).
   - API: `bool ListDriver::needsPass(double offset, const Geometry&)`, which stores the last geometry and `OffsetBand`, and `void ListDriver::noteOffset(double offset, ScrollPhase)`, which sets userScrolled. The view keeps only reading its own sizes.
   - Lines removed: iOS about -20, KT about -25.
   - Risk: thresholds differ. iOS uses `OFFSET_MOVED_THRESHOLD` (0.5 pt, 546) and KT uses `< 1` px (458). Make it a driver parameter.

5. **Reset after a cross-size change** (`resetCoreKeepingPosition`)
   - iOS 772-785. ENG 236-248, KT 550-559.
   - Already the same. iOS calls `_driver.resetKeepingPosition()` and keeps its own band/needsFrame/structure bump. Lines removed: iOS about -8.

## Measurement

1. **measureWindow / measureIndex with remeasure keys + pinned sticky measure**
   - iOS 690-740 (lambda 707-724). ENG 193-234.
   - API: already `ListDriver::setMeasureItem(std::function<double(size_t index, double cross)>)`. iOS passes a block that calls `sizeForItemAtIndex:` or `measureCellAtIndex:key:cross:` (746-759, stays in iOS). `markRemeasure(indexes)` replaces iOS `_remeasureKeys`.
   - Lines removed: iOS about -45.
   - Risk: iOS reads `[self coreOffset]` for the sticky lookup (730). ENG uses the pass offset. They are equal today because the write comes after the measure, but keep that order. The iOS measure callback needs the key, which the driver can pass (`measureItem(index, key, cross)`). That avoids `_keys[index]` on the platform side.

2. **Size commit tail is duplicated with Fabric**
   - ENG 211-215, iOS 735-739 and host/ListLayout.cpp:59-63 (`applyMeasuredRows`, used by the Fabric shadow node at ShadowListViewShadowNode.cpp:220) all do "lowest changed, then commitElementSizes, then recomputeTotalSize".
   - API: `struct SizeBatch { bool apply(Container&, size_t, Size); void commit(Container&); }` in ListLayout.hpp. The column cross-size override (ListLayout.cpp:43-49 vs ENG 226) should live in one place.
   - Lines removed: about -10 total. Low risk.

3. **Window range (measurement window, low/high clamp)**
   - iOS 693-700 **and again** 806-813. ENG 263-274.
   - API: `std::optional<IndexRange> measuredWindow(const Revision&)` in ListLayout.hpp. Lines removed: iOS about -14.

## Keys

1. **insert/delete splicing + structure bump**
   - iOS 455-498. ENG 50-82. Android also mirrors the list in Kotlin: KT 345-368 keeps its own `keys` ArrayList in sync by hand.
   - API: `ListDriver::insertKeys(sorted indexes, keys)`, `deleteKeys(sorted indexes)` and `setKeys`. iOS drops `_keys` and `_keysChanged`. Add `const std::string& keyAt(size_t)` so iOS does not keep a second copy.
   - Lines removed: iOS about -40.
   - **Bug (Android)**: `deleteItems` sorts but does not dedupe (KT 362). With a duplicate index, Kotlin `removeAt` removes **two different rows** (363-365). ENG `deleteKeys` (74-80) stops matching after the duplicate, because `indexes[removed]` stays on the duplicate value, so later indexes are never deleted. The Kotlin and engine key lists then disagree. Negative indexes crash `removeAt(-1)` in Kotlin, while JNI `readIndexes` silently drops them (JNI 67-69). For inserts this misaligns `indexes[i]` against `keys[i]` (ENG 54). iOS is immune because NSIndexSet is sorted and unique. Fix it once in the host: sort, unique and drop out-of-range values inside `insertKeys`/`deleteKeys`.

2. **Duplicate sticky index state on Android**
   - KT 126-132 keeps a sorted `stickyIndexes` copy and gates `layoutSticky` on it (739). ENG `sticky_` (ENG 35-38) holds the same data. Not a bug, but a source of drift. Add `ListDriver::hasSticky()`.

## Mounting

1. **Mount-range selection, masonry overlap and the "mount unchanged" early-out**
   - iOS 802-842 + 869-880 (overlap tested twice, 823-833 and 872-877). KT 565-613 + 615-623.
   - API: `host/MountPlan.{hpp,cpp}`:
     ```
     struct MountPlan { long low = -1; long high = -1; long sticky = -1; long held = -1; bool unchanged = false; };
     /*
      * Rows overlapping offset +/- pad from the measured window. In a masonry grid rows inside
      * the index range can be out of view, isMounted skips them.
      */
     MountPlan planMount(const Container&, bool horizontal, double offset, double windowAlong, double mountOverscan, ...);
     bool MountPlan::isMounted(const Container&, long index) const;
     ```
     plus a small `MountStamp { low, high, sticky, geometryVersion, structureVersion }` with `bool sameAs(const MountStamp&)`.
   - Lines removed: iOS about -40, KT about -35.
   - Risk (Android perf): KT deliberately avoids JNI in the in-band path. It mounts from a copied `windowFrames` array (KT 520-535). Moving the plan into C++ adds one JNI hop per scroll frame. Return it packed in a `jlong` or a reused `IntArray`, or keep the Kotlin loop and share only the rule. Measure it with the SLBench/Android bench.
   - **Likely bug (both)**: the unchanged key (iOS 834-836, KT 603-606) does not include the **active sticky index**. Scrolling up past header B's leading edge makes header A, far above the mounted range, the active one. If low/high/geometry are unchanged on that frame, A is never mounted, and `layoutSticky` finds no cell (iOS 953-955, KT 741), so the header disappears until the range shifts. Add `sticky` to `MountStamp`.

2. **frameOfElement / frameAt / frameOf**
   - iOS 789-796. ENG 276-285. host/ListLayout.cpp:66-80 `rowFrame` (Fabric) is the same switch, minus height and cross size.
   - API: extend `RowFrame` to a `RowRect rowRect(const Container&, size_t, bool horizontal, double windowCross)` in ListLayout.hpp. Fabric keeps `rowFrame` or wraps it. Lines removed: iOS about -8.

3. **Generation sweep / key-to-cell map** (algorithm only)
   - iOS 843-900, KT 616-665. The logic is the same: generation stamp, `appearing = generation == 0`, sweep stale entries. It holds platform cells and cannot move as-is.
   - Option: the host returns the index list to mount, and each platform keeps its own 20-line map. Do not template the map in C++, because the gain is small. Align the names (see Naming).

4. **Held row in the mount plan has a side effect (Android)**
   - KT 627 calls `drag.heldIndex()` → `core.dragHeldIndex()` → ENG 399-405, which calls `drag_.updateOrigin`. Every mount pass mutates the drag state. iOS 886-890 uses a pure `findElementIndexByKey`. Split it: `heldIndex() const` versus `syncHeldOrigin()`.

## Sticky

1. **activeStickyIndex binary search + push-up leading**
   - iOS 918-941 + 967-973. ENG 322-348. Fabric has two older forms: host/MountedRange.cpp:116 `activeStickyIndexFor` (linear) and host/StickyLayout.cpp:49 `sectionOverlayPosition` (linear, over offset arrays; used by Fabric iOS ShadowListView+Sticky.mm:97 and Fabric Android ShadowListGeometryJNI.cpp:168).
   - API in StickyLayout.hpp:
     ```
     long pinnedSectionIndex(const Container&, const std::vector<std::size_t>& sticky, double offset, bool horizontal);
     double pinnedSectionLeading(const Container&, const std::vector<std::size_t>& sticky, long active, double offset, bool horizontal);
     ```
     Then `ListDriver::activeStickyIndex`/`stickyLeading` forward to them.
   - Lines removed: iOS about -30.
   - Risk: the semantics differ from Fabric's `sectionOverlayPosition`. It clamps `offset >= 0` and returns `translation = offset`, because it is an overlay. The SLK version returns `max(leading, offset)`, because the cell is pinned in place. Keep both, built on one shared search. The binary search assumes `sticky` is sorted. ENG sorts (36), but iOS only relies on NSIndexSet ordering (352-354). That is fine today but must be a stated contract of the host function.

## Drag

1. **dragRowAtIndex + begin/beginCell + place/placeCell + insertion rows**
   - iOS 1088-1101, 1173-1178, 1203-1231. ENG 374-429. Fabric iOS repeats the columns branch: ShadowListView+DragReorder.mm:164-172 and 255-270.
   - API: move the engine methods into `ListDriver` (`dragBegin`, `dragHeldIndex`, `dragPlace`, `dragUpdateInsertion`). Also add to DragReorder.hpp, so that Fabric drops its branches:
     ```
     DragRow dragRowAt(const Container&, std::size_t index, bool horizontal, bool grid);
     void DragReorder::begin(const DragRow& resting, double touchAlong, double touchCross, std::size_t columns);
     DragOffset DragReorder::placeRow(double touchAlong, double touchCross, const DragRow& resting, double contentExtent, double crossExtent);
     ```
   - Lines removed: iOS about -45, Fabric iOS about -15.
   - Risk: iOS builds insertion rows from `_mounted` and **includes the held cell** (1225-1229). KT `mountedIndexes()` does the same. Fabric skips the dragged view (DragReorder.mm:282-284). Confirm that `DragReorder::updateInsertion` ignores the origin row. Otherwise the two hosts drop differently.

2. **Auto-scroll as our own write versus a user scroll**
   - iOS 1258-1268 `writeCoreOffset` (it sets `_lastOffset`, so the move never counts as user scrolling). DRG 179-190 `writeOffset` does the same. Fabric iOS DragReorder.mm:225-229 says explicitly that this _must_ count as a user scroll, or "blank rows during the drag".
   - In SLK the band check still triggers passes, so rows may be fine. But `userScrolled=false` changes the core's anchor/MVCP decisions while it auto-scrolls. Verify that it is intended, then make it explicit in the driver: `noteOffset(offset, phase, byUser)`.

3. **Fabric Android reimplements DragReorder in Java**
   - ShadowListDragController.java:248, 423-447, 538 calls free functions through ShadowListGeometry JNI (`dragHeldLeading`, `dragGridShifts`, ...) instead of the `DragReorder` class. After this work, a handle-based JNI wrapper like SLKCore's drag calls could replace that state machine. This is a later step.

## Commands

1. **Animated scroll-to target and landing** (`ScrollLanding`)
   - iOS 181-184, 1018-1022, 1057-1068, 1356-1413. KT 202-205, 843-856, 920-953.
   - API in a new host/ScrollLanding.hpp:
     ```
     /*
      * An animated command scrolls to an estimate. When the animation ends, the core lands
      * exactly on the row. A gesture cancels the landing.
      */
     struct ScrollLanding { long index = -1; double viewPosition = 0; bool end = false;
       void cancel(); bool pending() const; void land(Container&); };
     double animatedTargetOffset(const Container&, size_t index, double viewPosition, double windowAlong, double maxOffset, bool horizontal);
     ```
   - Lines removed: iOS about -25, KT about -20.
   - **Likely bug (both)**: an animated `scrollToStart` calls `scrollToItem(0, 0, animated)` (iOS 1388, KT 938) and lands through `core.scrollToIndex(0,0)`, not `core.scrollToStart()`. With a header, the animated path ends with row 0 at the top and the header scrolled off. The non-animated path shows the header. Add `ScrollLanding.start`.
   - **Risk (iOS)**: non-animated commands do not stop momentum. KT `runCommandNow` calls `gesture.stop()` (956). iOS 1380-1382 only invalidates, so a decelerating UIScrollView keeps writing contentOffset over the command. Add `[self setContentOffset:self.contentOffset animated:NO]` first, as Fabric does (ShadowListView.mm:1068).

2. **visibleRange packing**
   - iOS 1335-1344, ENG 301-310, JNI 259-267 (packed into a jlong), KT 896-901. The `long& low, long& high` out-params repeat in `windowRange`.
   - API: `std::optional<IndexRange> visibleRange() const` on the driver, with one `IndexRange{low, high}` struct (MountedRange in MountedRange.hpp already has that shape, so reuse it). Lines removed: iOS about -8. Low risk.

3. **Snap target** is already shared through Snap.hpp. iOS 1028-1041 and ENG 364-370 are thin wrappers. Nothing to do beyond routing iOS through the driver.

## Naming (align these)

1. Engine/driver: `resetKeepingPosition` (ENG) vs `resetCoreKeepingPosition` (iOS 772, KT 550). `activeStickyIndex(offset)` (ENG) vs `activeStickyIndexAt:` (iOS 918). `frameAt` (ENG) vs `frameOfElement:` (iOS 789) vs `frameOf` (KT 670) vs `rowFrame` (ListLayout). Pick `rowRect`/`activeStickyIndex`/`resetKeepingPosition`.
2. Offset helpers: iOS `coreOffset`/`writeCoreOffset:`/`maxCoreOffset` (282-306) vs KT `offset`/`writeOffset`/`maxOffset` (219-232). Axis: iOS `along:`/`crossOf:` (271, 1083) vs KT `along(x,y)`/`cross(x,y)`. Use `offset`/`writeOffset`/`maxOffset` and `along`/`cross` on both.
3. Layout pass: iOS has no named pass (inline in `layoutSubviews`, `_inLayout`), while KT has `layoutPass()`/`inLayoutPass`. Drag: iOS `_dragCell`/`_dragKey`/`beginDragAt:`/`updateDrag`/`endDrag:`/`applyDragShiftsAnimated:`/`dragTick:` vs KT `heldCell`/`begin`/`update`/`end`/`applyShifts`/`doFrame`. ENG mixes the terms (`dragKey_` vs `dragHeldIndex`). Standardize on "held".
4. Constants: iOS `SLK_MAX_PASSES` is duplicated (iOS 38, ENG 12), and the settling cap is the literal `60` on iOS. They move to the host as `MAX_LAYOUT_PASSES`/`MAX_SETTLING_PASSES` in Constants.hpp. Drag constants: iOS `SLK_DRAG_LIFT` vs DRG `LIFT`, and the durations are 0.2/0.22/0.25 s on iOS vs 150/220/250 ms on Android (the lift duration differs: 200 vs 150 ms).
5. Files and packages: iOS keeps `SLKListCell`, `SLKDelegateProxy`, drag and list in one SLKListView.mm. Android splits them (SLKListCell.kt, SLKDragController.kt, SLKScrollGesture.kt). Split iOS into SLKListCell.mm and SLKListView+Drag.mm (matching Fabric's `ShadowListView+DragReorder.mm`). Package dirs are `shadowlist-uikit` vs `shadowlist-android`, which names a toolkit on one side and a platform on the other. Consider `shadowlist-ios`/`shadowlist-android`, or `-uikit`/`-androidview`. The public API is idiomatic per platform (`insertItemsAtIndexes:` vs `insertItems`), which is fine. One real mismatch: Android `Sizing.sizeForItem` takes `crossSize: Int`, while iOS uses `CGFloat`.

## Splits (functions doing too much)

1. iOS `layoutSubviews` (520-569): split into `readGeometry`, `trackUserScroll` and `layoutPass`, like KT 421-461.
2. iOS `runFrames` (606-684): builds FrameInput, runs the loop, clamps, writes the offset and schedules the settle. Most of it moves to the driver. The rest becomes `applyPassResult`, like KT 508-518.
3. iOS `mountCells` (802-901): range choice, early-out, mount lambda, sticky, held row and sweep in one method. Split it the way KT does: `mountCells`/`mountUnchanged`/`mountRange`/`mount`/`recycleStale` (584-658). KT `mountUnchanged` is a predicate that also records state. Rename it to `recordMount` or split it.
4. iOS `beginDragAt:` (1156-1193): hit test, permission, core begin, lift styling, haptics, display link. iOS `endDrag:` (1270-1307): commit, reload, fly-back. Split into `heldCellAt:`, `liftCell:` and `dropCell:` (KT already has `lift`/`drop`, DRG 110, 223).
5. ENG `dragHeldIndex` (399-405): a query that mutates. Split into `heldIndex() const` + `syncHeldOrigin()`. ENG `runFrames` also packs the result and resets the reached flags. Optionally split into `runPasses` + `finishPass`.

## Fabric RN hosts that could reuse this later

- Sticky search + push-up: StickyLayout `sectionOverlayPosition` (Fabric iOS +Sticky.mm:97, Fabric Android GeometryJNI:168) can share the new search.
- DragReorder `begin`/`placeRow` unified: Fabric iOS DragReorder.mm:164-172, 255-270. Fabric Android drag state can move to the class (see Drag 3).
- `SizeBatch`: ListLayout `applyMeasuredRows` (ShadowNode.cpp:220). `rowRect`: ShadowNode.cpp:298.
- `Container::hasPendingCommand()`: Container.cpp:301 and the Fabric shadow node.
- The frame loop itself does **not** apply. Fabric settles through shadow-node commits (ShadowNode.cpp:348) and ScrollSync, not a synchronous pass loop.

## Migration order

| #   | Step                                                                                                                                                                                                                                                              | Effort |
| --- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------ |
| 1   | Add `Container::hasPendingCommand()`, `SizeBatch`, `rowRect`, the `measuredWindow` helper and `IndexRange` in core and ListLayout. Switch `computeOffsetBand` and `applyMeasuredRows` to them. Run the core tests and the fuzz harness.                           | 2-3 h  |
| 2   | Move SLKEngine to `host/ListDriver.{hpp,cpp}` (rename, drop the SLK prefix and `kit` namespace, sanitize indexes in `insertKeys`/`deleteKeys`, add `keyAt`/`hasSticky`). Repoint JNI/SLKCore.kt and fix the Kotlin delete-duplicate bug. Build Android.           | 2-3 h  |
| 3   | Port iOS SLKListView.mm onto `ListDriver`: keys, measurement, passes, reached flags, reset, sticky, drag math, visibleRange. Fix the iOS settle scheduling (dedupe flag) and stop momentum on non-animated commands. Run SLBench against the 2026-10-07 baseline. | 4-6 h  |
| 4   | `StickyLayout` pinned-section functions + `MountPlan`/`MountStamp` (add sticky to the stamp: fixes the vanishing-header case on both). Measure the Android JNI cost on the in-band path before keeping the C++ plan there.                                        | 3-4 h  |
| 5   | `ScrollLanding` (fixes the animated `scrollToStart` header case) + DragReorder `begin`/`placeRow`/`dragRowAt` overloads. Switch Fabric iOS drag to the overloads.                                                                                                 | 2-3 h  |
| 6   | Naming alignment + iOS file split (SLKListCell.mm, SLKListView+Drag.mm) + the function splits above.                                                                                                                                                              | 2-3 h  |
| 7   | Later: Fabric Android drag on the `DragReorder` class, Fabric sticky on the shared search.                                                                                                                                                                        | 3-5 h  |

Total for steps 1-6 is about 15-22 h. Each step builds and ships on its own.

## Done (2026-10-07)

Steps 1 to 6 are in, without the Fabric parts. packages/shadowlist-fabric is unchanged.

- `host/ListDriver.{hpp,cpp}` holds what SLKEngine did, plus the iOS-only parts: `ListSettings`,
  `PassInput`, `PassResult`, `RowRect`, `MountPlan` (`planMount`, `shouldMount`), `ScrollLanding`
  (`setLanding`, `land`, `cancelLanding`, `animatedTargetOffset`), `measuredWindow` and
  `visibleRange` as `std::optional<MountedRange>`, `keyAt`/`keyCount`/`hasSticky`, `stickyLeading`,
  and the drag calls named for the held row (`heldIndex`, `placeHeld`, `dragEnd`, `dragShiftFor`).
  `insertKeys`/`deleteKeys` sort, dedupe and drop out of range indexes. The measure callback
  gets the key. `MAX_CORE_PASSES_PER_LAYOUT` and `MAX_SETTLING_LAYOUTS` replace the two literals.
- Core helpers: `Container::hasPendingCommand()` (used by `computeOffsetBand` and the driver),
  `SizeBatch` in ListLayout (used by `applyMeasuredRows` and the driver). Tests are in
  packages/shadowlist-core-tests/tests_host_list_driver.cpp.
- Android: SLKEngine.\* is deleted. SLKCoreJNI.cpp wraps `ListDriver`. The in-band scroll frame
  still makes no JNI call: Kotlin mounts from the copied window frames and pins from the copied
  sticky frames, with the same rule as `planMount`.
- iOS: SLKListView.mm (1415 lines) is split into SLKListView.mm (1064), SLKListView+Drag.mm
  (284), SLKListCell.mm (18) and two headers in Internal/ that stay out of the public headers.
  Keys, passes, measurement, reached flags, reset, rects, mount plan, sticky, landing, drag rows
  and visibleRange all go through the driver.
- Bugs fixed. iOS: settle frames wait for the next display frame behind one flag. Non-animated
  commands stop momentum and any animated command first. The mount skip includes the active
  sticky index. Animated scrollToStart lands through `scrollToStart`. Both: drag auto scroll
  counts as the user's scrolling, the same as Fabric. Android already had its fixes for the
  sticky stamp, scrollToStart and duplicate deletes. The driver now also guards the last one.
- Naming: `runPasses`/`applyPassResult`, `resetKeepingPosition`, `rowRect`, `offset`/
  `writeOffset`/`maxOffset`, `along`/`cross`, `heldCell`/`heldIndex` on both platforms. The JNI
  slots are `PASS_*`.

Left as they are on purpose:

- The band check stays in each view. On Android a driver call there would add a JNI hop to
  every scroll frame.
- The moved threshold is 0.5 pt on iOS and 1 px on Android. Android offsets are whole pixels.
- `Sizing.sizeForItem` takes an Int cross size on Android, the platform's unit.
- The lift lasts 0.2 s on iOS and 150 ms on Android. Both are now named constants.
- Fabric Android's drag stays in Java on the free DragReorder functions (Drag 3). It does not
  use the class, so the overloads do not reach it yet.

## Done (2026-10-07, later)

Step 7 for the parts that apply to Fabric, and the iOS pixel rounding:

- Sticky: `StickyLayout.hpp` has the shared search, `pinnedSectionPosition` (a binary search
  over leading edges read through a callback) and `pinnedSectionLeading` (pinned at the offset,
  never above its place, pushed up by the next header). `sectionOverlayPosition`, which Fabric
  iOS (ShadowListView+Sticky.mm) and Fabric Android (ShadowListGeometryJNI.cpp) call, is built on
  them instead of its own scan. So are `ListDriver::activeStickyIndex`/`stickyLeading` and
  `activeStickyIndexFor` in MountedRange. The Kotlin list keeps its own copy of the search on
  purpose: it pins from copied frames without a JNI call per scroll frame.
- Drag: `DragReorder::begin(const DragRow&, touchAlong, touchCross, columns)` and
  `placeRow(...)` pick the list or grid call by themselves. Fabric iOS
  (ShadowListView+DragReorder.mm) and `ListDriver` use them and dropped their own branches.
- `Container::hasPendingCommand()` and `SizeBatch`: Fabric has no copy of the pending command
  predicate. It gets it through `computeOffsetBand`. `SizeBatch` reaches it through
  `applyMeasuredRows`. Nothing else to remove.
- Keys: the core reconciles appends, prepends and trims at either end in place, updates the key
  map in place for other changes without duplicate keys, and takes a `KeyEdit` hint
  (`FrameInput::keyEdit`) for one end edit. `ListDriver` records that edit, splices its own key
  list in place for a few runs (`MAX_IN_PLACE_KEY_RUNS`), and has `replaceKeys`/`reloadKeys`
  for hosts that diff a reload. Tests: tests_reconcile_edges.cpp, tests_host_list_driver.cpp,
  tests_host_shared_helpers.cpp.
- iOS writes offsets rounded to device pixels and reports the exact offset back while the
  view rests there. Rows draw shifted by the difference with pixel aligned edges. The prepend
  check reports 0 (below 1e-11 pt) instead of 0.05 to 0.08 pt.
