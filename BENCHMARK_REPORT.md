# Prediction pipeline + optimization pass — measured results

## Headline

**Scrolling is not the bottleneck, at any list size I could measure.** The core resolves a
scroll frame in **0.5 µs at 100,000 rows**. On device the list holds a locked 60 fps with
zero blank cells and 13–14 % of one core, and it does so identically before and after every
change in this pass.

The costs that are real are **data mutations**, not scrolling:

| Scenario (100k rows, core, Release, M-series) | Cost |
| --- | ---: |
| reconcile alternating append-one/remove-one | **6.1 ms** |
| prepend 30 rows while scrolled (chat) | **6.3 ms** |
| cold first update | 2.8 ms |
| copy long keys into a new FrameInput | 1.8 ms |
| DRAG scrollbar top to bottom | 47 µs/frame |
| FLING vertical / inverted / horizontal | 0.3–0.6 µs/frame |

A 6 ms reconcile is a dropped frame every time a chat appends a message or a page paginates
in. **That is where this pass went, and it is now 1.8× faster** (6,134 → 3,430 µs at 100k);
prepend is 1.66× faster (6,240 → 3,752 µs). Details below.

## What was built

An ahead-of-time row measurement pipeline, so a row's height is known before React renders
it rather than discovered afterwards and corrected.

- `measureElement` prop on `<ShadowList>` returning a `MeasureSpec` per row.
- Native measures through `TextLayoutManager` — the same engine the real layout uses. The
  measure cache key ignores node identity, so the prediction and the later real measurement
  share one cache entry.
- Core carries predictions as first-class geometry (`Element::predicted`,
  `Container::predictedSizes`, `Virtualizer::applyPredictedElementSize`).
- The commit hook's materialization band now accepts predicted rows
  (`hasTrustedSize`), so a row can be dematerialized before it has ever been mounted.

16 tests in `packages/shadowlist-core-tests/tests_prediction.cpp` pin the contract. The
headline one: with exact predictions, 400 rows report **0** geometry changes when the real
measurements arrive; the same list without predictions reports **300+**.

## Benchmark methodology

Android emulator (`Medium_Phone_API_35`, arm64-v8a, 1080×2400), Release APKs, animations
disabled, `MODE=sweep` (60 unidirectional flings, ~27 s).

`packages/shadowlist-core-bench/device-metrics.sh` + `analyze-metrics.py` were written for
this pass and measure, per run:

- **Frames** — `gfxinfo framestats`, parsed by column NAME (the schema has gained columns
  across Android releases; fixed indices silently yield zero usable frames).
- **Blank cells** — the tallest FULL-WIDTH band of background inside the list viewport,
  sampled from the raw framebuffer. Counting background *pixels* does not work: a chat
  bubble is capped at 75 % width, so a healthy list reads as ~40 % "blank". A missing row is
  a band spanning the whole width, hundreds of pixels tall; row padding is a few pixels.
- **Scroll verification** — a viewport fingerprint per sample, reporting the share of frames
  that actually changed. Every number below comes from a run at **97–100 %**, so the list
  genuinely travelled.
- **Memory** — PSS, native heap, live View count.
- **CPU** — `utime+stime` from `/proc`, as a share of one core.

## Device results — HEAD vs working tree, Chat, 20k rows, deep sweep

| | p50 | p90 | p99 | blank worst | frames >200px blank | PSS | Views | CPU |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| baseline (HEAD) | 17.09 | 17.52 | 19.07 | 28 px | 0 | 254 MB | 449 | 14.2 % |
| current | 17.21 | 17.77 | 20.87 | 28 px | 0 | 262 MB | 449 | 13.8 % |
| current + this pass | 17.23 | 18.17 | 20.74 | 28 px | 0 | 263 MB | 449 | 13.0 % |

Indistinguishable. p50 sits at the 16.7 ms vsync interval in all three; there are no blanks
to remove and no jank to fix.

Earlier runs at 1,000 rows on a physical Nothing Phone (1) showed the same shape: four runs,
all within run-to-run noise of each other, with within-build variance larger than
between-build variance.

**Later device runs in this session are invalid and are not reported.** A game was running at
611 % CPU with a host load average of 11.4, and the emulator was starved: the same
known-good APK that measured p50 17.4 ms measured 51.5 ms with 26 % janky frames under that
load. This was caught by re-running the unchanged control, not by assuming. Every core
benchmark above was re-verified by measuring both builds back to back under that same load —
the ratios held to within a few percent (1.75× / 1.65× / 82× against 1.79× / 1.66× / 84×), so
the core conclusions are load-independent. The device comparison needs a re-run on an idle
machine.

## Optimizations applied this pass

### Data mutation — the real win

Medians of 3 runs each, host Release, same session, measured against a reverted copy of the
current tree so the attribution is exact.

| Scenario | rows | Before | After | Change |
| --- | ---: | ---: | ---: | ---: |
| reconcile alternating append-one/remove-one | 1,000 | 46.6 µs | 26.8 µs | **1.74× faster** |
| reconcile alternating append-one/remove-one | 10,000 | 562 µs | 311 µs | **1.81× faster** |
| reconcile alternating append-one/remove-one | 100,000 | 6,134 µs | 3,430 µs | **1.79× faster** |
| prepend 30 rows while scrolled (chat) | 100,000 | 6,240 µs | 3,752 µs | **1.66× faster** |
| cold first update | 100,000 | 2,806 µs | 2,657 µs | 1.06× faster |

`reconcileElements` was rebuilding the whole element vector *and* the whole key→index map on
every data change. The map rebuild dominated: one heap node allocated and one freed per row,
plus a hash and a probe each. Three changes removed most of it.

1. **In-place append path.** Detect the shape with one forward pass of string compares (tens
   of µs at 100k, against milliseconds to rebuild), then extend the existing vector and map.
   Surviving rows keep their indices, so every existing map entry stays correct.
2. **In-place prepend path** — the shape a chat pagination takes. The whole list survives but
   every index shifts, which used to force a full rebuild. Now the vector shifts (it is
   contiguous; unavoidable) and the map is left completely untouched.
3. **`Revision::indexBias`.** Prepending K rows raises every index by K. Instead of walking
   the map adding K to each value — an O(rows) chase through scattered nodes — the bias is
   shifted once, in O(1), and every value is read back through it. Encapsulated behind
   `indexForKey` / `setIndexForKey`; a raw read of a node would return a wrong index
   silently, so the raw map is documented as off-limits.
4. **Deleted `prevElementConsumed`.** The `vector<bool>` consumed bitmap is provably
   redundant: `emplace(...).second` already reports whether this is a key's first
   occurrence, which is exactly when a previous element may be claimed. −1 allocation and
   −200k bit operations per reconcile.
5. **Killed a double `Element` move** per survivor (move straight into place rather than via
   a named local): −100k moves of a 112-byte struct with two strings.

8 new tests cover these paths, including duplicate keys on both append and prepend, chained
prepends against the bias, and a shape the fast paths must decline.

### Predictions landing deep in the list — O(N) per frame becomes O(1)

The reflow changes looked worthless until a scenario existed for the path they actually
target. `benchDeepPredictions` stages sizes for rows near the end of the list, which is the
shape the `measureElement` pipeline produces: the host measures a screen or two ahead, so
the rows that change are nowhere near the front.

| rows | Before | After | |
| ---: | ---: | ---: | --- |
| 1,000 | 5.62 µs/frame | 2.82 µs/frame | 2.0× faster |
| 10,000 | 12.42 µs/frame | 1.17 µs/frame | **10.6× faster** |
| 100,000 | 97.18 µs/frame | 1.16 µs/frame | **84× faster** |

The old path scales O(N) per frame (5.6 → 12.4 → 97.2); the new one is flat. Three changes
together: reflow from the lowest changed row instead of always from row 0, a sound early exit
once past the changed span and offsets reconverge, and a `geometryVersion` bump only when an
offset actually moved.

Nothing else in the benchmark exercised this, which is why it read as "no measurable change"
until the scenario was written.

### Props deep-copy — 3× fewer republishes

Verified in React Native's own source: `convertRawProp` returns `sourceValue` **by value**
(`propsConversions.h:177`), so cloning props deep-copies every prop the update did not
mention. Each list prop change therefore copies the entire `elementsAllKeys` vector — the
bench measures that copy at **86 µs for 100k short keys and 1.88 ms for 100k realistic long
keys** — and changes the props pointer, which costs the core a full O(rows) key revalidation
on top.

With `measureElement` on, the specs prop was republished on a fixed 16-row quantum, so this
was being paid every 16 rows of scrolling. Republishing is now edge-triggered: the span is
re-cut only when the mounted window comes within 24 rows of its edge.

Scrolling 20,000 rows end to end: **2,492 → 821 republishes, 3.0× fewer**, at an identical
payload size per republish.

Raising `measureLookahead` to 120 cuts it further to 229 (10.9×), but makes each burst 2.5×
larger — 240 `measureElement` calls and a ~35 KB `JSON.stringify` in one tick. Left at 48
because the device measurement needed to choose between them was not available (see below).

The native side also now caches the PARSED specs, not just the measured ones: measurement is
spread over several commits, so a partially-worked prop was re-parsing its whole JSON payload
on each of them.

### Scroll path — still no measurable change

| Scenario (100k rows) | Before | After |
| --- | ---: | ---: |
| FLING vertical / inverted / horizontal | 0.53 µs/frame | 0.57 µs/frame |
| DRAG scrollbar top to bottom | 46.8 µs/frame | 46.4 µs/frame |

Hoisting the materialization band out of the commit hook's per-row loop (O(M²)→O(M)),
`dynamic_cast`→component-handle compares, `const auto&` on the per-commit state copy, and the
per-commit cap on text measurement are all sound and tested but moved no number. They are
simpler or strictly-better code rather than measured wins.

### Also fixed

`bench.cpp` bound `getSnapOffsets()` with `auto` instead of `const auto&`, copying 100,000
doubles on every timed op. That made the snap numbers meaningless and is why the stored
Sep-7 results show a fictitious "126× faster" when compared against.

## Caveats, stated plainly

- **I broke the example app's iOS pods** by running `pod install` with no network; it cleared
  76 of 79 pods before failing. Repaired later once network returned — 79/79 restored.
- **HEAD does not build for Android.** Commit `42d306d` declares `elementsAnchorIgnoreKeys`
  in the TS spec but never added the Java setter. The baseline worktree carries a 4-line
  no-op setter, marked `BENCHMARK-ONLY`, purely to make it compile. No behaviour change.
- **The stored `results/current.host.txt` (Sep 7) is not a usable baseline** — different
  session, and it predates the `getSnapOffsets` bench fix. Comparing against it produces a
  fictitious "126× faster" on snap offsets and an implausible uniform 1.6× on everything.
- **The emulator understates the problem.** Locked 60 fps, zero blanks, 13 % CPU. It cannot
  show a regression or an improvement in the scroll path.
- Reflow-from-index and the early exit are **kept but unproven**: sound, tested, lower
  worst-case complexity, no measured gain. They are complexity that has not yet paid for
  itself, and reverting them is defensible.

## Where to go next

1. **`reconcileElements` at 6.1 ms / 100k rows.** It rebuilds the whole `Element` vector and
   the whole `key → index` map on every data change. The common shapes — append, prepend,
   single insert/remove — should be detectable in O(changed) by comparing prefix and suffix
   runs, and handled in place.
2. **Props deep-copy.** Any prop change on the list deep-copies the full `elementsAllKeys`
   vector (1.8 ms at 100k) and invalidates the `keysUnchanged` proof, forcing a full O(N) key
   revalidation. With `measureElement` on, `elementsMeasureSpecs` changes once per 16-row
   quantum, so this fires during scrolling.
3. **Event coalescing.** RN's `EventQueue` stops at the first entry matching the target, so
   an alternating `visibleIndicesChange` / `scroll` stream coalesces nothing and the queue
   grows for the length of a gesture. One merged per-frame event would fix it.
4. **A lazy-tail or Fenwick offset index** would make DRAG O(log N) instead of O(N). At
   47 µs/frame it is 0.3 % of a frame budget — not worth the risk today.
