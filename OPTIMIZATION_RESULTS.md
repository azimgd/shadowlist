# ShadowList performance work: measurements, changes, and results

Date: 2026-09-07
Baseline revision: `42d306d` (`chore: release 0.6.3`)
Device: Nothing Phone (2a) (`A063`, arm64, Android 15) for core measurements; iOS Simulator (iPhone 16 Pro, iOS 18.6) for end-to-end app verification.

This implements the optimizations identified in [PERFORMANCE_REPORT.md](PERFORMANCE_REPORT.md) and
[FABRIC_PIPELINE_REPORT.md](FABRIC_PIPELINE_REPORT.md), adds the measurement harness that makes
before/after comparable, and adds a test suite that pins the virtualization contract those
optimizations had to preserve.

## Headline

On the Nothing Phone, the core work behind **one scroll frame of a 100,000-row list** went from
**28.3 ms to 1.4 µs**. The 60 Hz frame budget is 16.7 ms, so that single number is the difference
between "cannot hold a frame at all" and "not measurable against the budget".

The reason the ratios look absurd is not micro-tuning: the per-frame cost **stopped scaling with
list size**. Before, a scroll frame was `O(rows)` several times over. After, it is `O(mounted
window)`, so the same frame costs 1.4 µs whether the list holds 1,000 rows or 100,000.

| Scroll frame, core work only | 1,000 rows | 10,000 rows | 100,000 rows |
| ---------------------------- | ---------: | ----------: | -----------: |
| Before                       |    58.5 µs |    890.9 µs |  28,271.3 µs |
| After                        |     1.4 µs |      1.4 µs |       1.4 µs |

Two correctness defects were found and fixed along the way (see [Bugs fixed](#bugs-fixed)),
including a use-after-free that crashed the app under aggressive scrolling.

## How to reproduce

```bash
# Core benchmark: builds for this host AND cross-compiles for the connected
# Android device, runs both, writes results/<label>.{host,device}.txt
packages/shadowlist-core-bench/run.sh optimized
packages/shadowlist-core-bench/compare.py \
  packages/shadowlist-core-bench/results/baseline.device.txt \
  packages/shadowlist-core-bench/results/optimized.device.txt

# Virtualization contract tests
cmake -S packages/shadowlist-core-tests -B build && cmake --build build && ./build/shadowlist_core_tests

# App-level frame timings on a connected Android device (gfxinfo framestats)
packages/shadowlist-core-bench/device-scroll.sh <label> Feed
```

The baseline column was produced by running the **same benchmark binary** against a git worktree
at `42d306d`. The benchmark source compiles against both revisions (it feature-detects the two
`FrameInput` fields the optimized core adds) and each revision is measured doing what its own
Fabric integration would do, so the two runs are directly comparable rather than one being
handicapped.

## What the benchmark measures

The scenarios are not synthetic loops around one function; each drives the core through the call
sequence the Fabric integration actually performs.

The most important are the `FLING` rows: one iteration is one display frame of a fling — publish
the new scroll offset via `Virtualizer::update`, then feed back the measured size of every mounted
row exactly as `ShadowListViewShadowNode::layout` does. Those scenarios **assert that they mounted
a window's worth of rows** and print a warning otherwise, so a result that got fast by quietly
doing nothing cannot be reported as a speedup. (That guard immediately caught a bad horizontal
fixture during development.)

## Results: Nothing Phone (2a), 100,000 rows

| Scenario                                    | Unit       |    Before |    After |    Change |
| ------------------------------------------- | ---------- | --------: | -------: | --------: |
| FLING vertical single column                | us/frame   |  28,271.3 |      1.4 |   20,208x |
| FLING inverted (chat)                       | us/frame   |  28,336.1 |      1.4 |   20,269x |
| FLING horizontal                            | us/frame   |  13,759.6 |      0.8 |   17,351x |
| FLING 3-column grid                         | us/frame   |  76,709.9 |      3.6 |   21,028x |
| FLING vertical, deep in list                | us/frame   |   7,263.9 |      1.5 |    4,708x |
| inverted list: jump to top (tap status bar) | us/jump    |   5,284.8 |      0.6 |    8,750x |
| 20 unchanged measurements at start          | us/batch   |  25,058.4 |      1.0 |   24,305x |
| 20 unchanged measurements at midpoint       | us/batch   |   9,865.6 |      1.0 |    9,569x |
| warm top update, partially estimated        | us/op      |   3,412.0 |      0.3 |   12,498x |
| warm deep update, fully measured            | us/op      |   2,032.1 |      0.3 |    6,912x |
| generate snap offsets                       | us/op      |     801.1 |     72.3 |     11.1x |
| construct and destroy N Elements            | us/op      |  32,406.9 |  4,339.7 |      7.5x |
| cold first update                           | us/op      |  67,651.9 | 40,254.8 |      1.7x |
| prepend 30 rows while scrolled (chat)       | us/prepend | 138,462.9 | 99,048.7 |      1.4x |
| reconcile alternating append/remove         | us/op      | 128,273.5 | 97,966.9 |      1.3x |
| copy long keys into new FrameInput          | us/op      |  27,509.8 | 27,485.8 | unchanged |

Full tables for 1,000 / 10,000 / 100,000 rows on both device and host are in
`packages/shadowlist-core-bench/results/`.

Two rows deserve explanation:

**The key-copy rows are unchanged on purpose.** They measure the cost of copying the key
collection into a fresh `FrameInput`, which is what the baseline integration did on every commit.
The optimized integration no longer does it at all — it lends the core the props' own vector — so
that 27.5 ms per commit on a 100k long-key list is _eliminated_ rather than made faster. The
benchmark keeps measuring the copy so the size of what was removed stays visible.

**Reconcile and prepend improve only ~1.3-1.8x.** A prepend genuinely changes every row's index,
so it is inherently `O(rows)`. The gain is from not building a throwaway key→Element map. Making
this sublinear needs a different data representation and was out of scope.

## Changes

### Core (`packages/shadowlist-core`)

**F1 — unchanged size feedback no longer reflows the list.** `updateElementAtIndex` split into
`applyElementSize` (records, reports whether geometry actually moved) and `commitElementSizes`
(propagates once). Fabric feeds back a size for _every_ mounted row on _every_ layout, and in a
steady scroll essentially all of them are unchanged; each one used to reflow the entire suffix
behind it. A mounted window near the start of a 100k list visited ~2 million elements per layout
to record sizes that had not moved a pixel. This is the single largest win.

**F2 — the fallback sizing pass no longer runs every frame.** It assigns fallback dimensions to
unmeasured rows but does not mark them measured, so on a settled list it used to reassign the same
values to every far-away row every frame and, because each assignment counted as "newly
estimated", force a full offset reflow behind it. It now runs only when the fallback dimensions, a
layout parameter, or the element list actually changed.

**F3 — keys are borrowed, not copied, and not revalidated on a scroll.** `FrameInput::keysRef`
lets the integration lend its immutable props vector. `FrameInput::keysUnchanged` lets a host that
can _prove_ the keys are identical (Fabric can: a scroll clones the shadow node with new state and
the same props object) skip the `O(rows)` key comparison. The geometry cache holds a strong
reference to the previous props, so pointer identity is a sound proof rather than a guess.

**F4 — the window is found by search, not by scanning from row zero.** Rows are laid out end to
end, so `offset + size` is monotonic along a track and the first overlapping row is
binary-searchable. Single-column lists search once; multi-column lists search **per track** (rows
are assigned round-robin by `index % columns`, so each track is ordered even though global index
order is not). A grid previously scanned all N rows on every frame.

**F5 — snap and sticky geometry are cached against a geometry version.** `Container::geometryVersion`
is bumped wherever element positions change. `getSnapOffsets` (one target per row) memoizes
against it, and the Fabric layer caches the converted sticky/snap arrays the same way. None of
this depends on the scroll offset, which is the only thing that changed on almost every frame that
published it.

**F7 — reconcile reuses the existing key index.** It no longer builds a temporary
`key → Element` map that duplicated every key string and moved every 112-byte Element twice.
Duplicate-key and survivor-measurement semantics are preserved and tested.

**F8 — element debug ids are generated on first use.** `Element::id` was a `std::stringstream`

- `<random>` draw per row in the constructor; it is read only by debug JSON serialization. It is
  now `Element::getId()`, computed lazily with SplitMix64 over a thread-local counter.

**Correctness: one fallback size, one source of truth.** The measure pass used the raw estimate
while the layout pass used the frozen average, so simply bringing a row into the window _resized_
it, reflowing everything after it — after the window had already been chosen. Both now use
`effectiveFallbackSize`.

**Correctness: reflow before choosing the window.** On the frame a mutation lands, every element
still carried its pre-mutation offset. The window was therefore selected from where rows _used to
be_.

### Fabric integration (`packages/shadowlist-fabric/cpp`)

**F1/F10 — `layout()` is now two passes.** Pass 1 takes in every mounted row's measurement and
reflows once from the lowest row that actually moved. Pass 2 places rows, cloning a child only when
its frame really changed — Yoga only overwrites a child's metrics when it re-laid it out, so an
unmoved row still holds the frame we want, and re-cloning it bought nothing while costing a clone,
a shadow-node replacement, a Yoga child replacement and a spurious `onLayout` entry per row per
frame. Templates (header/footer/empty/section overlay) get the same treatment. Replacements now
pass the known child index, which both `ShadowNode::replaceChild` and
`YogaLayoutableShadowNode::replaceChild` otherwise find by linear search.

Placing rows after all measurements are in also fixes an ordering wrinkle: rows used to be
positioned against geometry that later rows in the same pass had not yet contributed to.

### JS (`packages/shadowlist-fabric/src`)

**Direction-aware overscan (fewer blank cells).** A blank cell during a fling is a row the native
side has already scrolled to but React has not mounted. What protects against it is runway _in
front of_ the user, not a bigger buffer behind. The mounted range now pads 10 rows on the leading
side and 4 on the trailing side, with direction taken from the window's own movement so inverted
and horizontal lists work without special-casing.

**`ElementRenderer` index dependency (correctness).** The child memo called
`renderElement({ element, index })` but omitted `index` from its dependencies, so a prepend or
reorder that kept the same item object left the row rendering content built from its old index.

## Bugs fixed

**A row could be dropped from the mounted window for a frame.** Because the measure pass and the
layout pass disagreed on what size to give an unmeasured row, visiting a row resized it and shifted
everything after it — after the window had been chosen. The row that shift pulled into range was
left unmounted until some later frame. This is caught by `randomized_session_never_loses_a_row`,
which **fails on the baseline and passes after the fix**.

**Use-after-free crash under aggressive scrolling.** Yoga records every re-laid-out child in
`LayoutContext::affectedNodes` as a raw pointer, and `ShadowTree::tryCommit` dereferences that list
_after_ layout finishes (`emitLayoutEvents` dynamic_casts each entry). `layout()` then replaces
those same children with repositioned clones; a child whose only owner was the slot we overwrote is
destroyed immediately, leaving a dangling pointer in a list the framework is still going to read.

This is latent in the baseline. Mounting more rows per commit (the overscan change above) made it
fire reliably: the app segfaulted inside `emitLayoutEvents` within seconds of hard scrolling. The
fix retains every child the layout swaps out until the next layout of that node
(`replacedChildren_`), which keeps the pointers valid for the rest of the commit that recorded
them. Verified by bisection: baseline survived 180 aggressive swipes; baseline + the overscan
change alone crashed 5 times in 120 swipes; the full change set with the lifetime fix survived 450.

## Correctness verification

The suite was one `CHECK(true)` placeholder. It is now 36 tests across two files.

`tests_virtualization.cpp` (28 tests) pins the contract implementation-agnostically, so **it also
passes against the pre-optimization core** — it encodes existing behavior rather than my
implementation. The window tests are exhaustive cross-checks: an independent brute-force pass
decides which rows overlap the viewport plus overscan, and the core's window must contain exactly
those. That is what makes replacing a linear scan with a binary search safe — a search that skips
a row shows up as a lost row, not as a subtle visual bug. Covered: single column, horizontal,
inverted, skewed multi-column tracks, rows straddling the window bound, reorder frames, geometry
contiguity, reconcile (append/prepend/trim/reverse/duplicate keys/full replacement), MVCP across
prepend, inverted bottom pin, inverted jump-to-top, `scrollToIndex`, content shrink, snap geometry,
viewability, estimation, and a 400-step randomized session asserting the whole contract after every
step.

`tests_batch_measurement.cpp` (8 tests) proves the batched intake path lands on **exactly** the
geometry the per-row path would have (offsets, total size, scroll offset, and the measurement
accumulators), plus the props-identity shortcut and borrowed keys behaving identically to the
owning path.

Baseline: 27/28 pass (the one failure is the dropped-row bug). Optimized: 36/36.

## Device verification

Driven with `agent-device` on the iOS Simulator, with the crash-report count checked after each
scenario:

| Scenario                                                              | Result                                                 |
| --------------------------------------------------------------------- | ------------------------------------------------------ |
| Feed, 450 aggressive swipes both directions                           | no crash, content correct, no blank cells              |
| Chat (inverted, 1,000 messages), 320 aggressive swipes                | no crash, returns to newest correctly                  |
| Chat, prepend x6 while scrolled (MVCP)                                | no crash, position maintained                          |
| Chat, `scrollToIndex` x6                                              | no crash, lands correctly                              |
| Nested (horizontal lists inside a vertical list), 120 h + 45 v swipes | no crash, content correct                              |
| Masonry (3 columns, uneven heights), 160 swipes                       | no crash, layout correct                               |
| Baseline app, same Feed stress (control)                              | no crash — confirming the crash above was a regression |

## Follow-up: scroll-indicator drag

Dragging the scroll indicator is a different workload from a fling, and the optimizations
above did not cover it. A fling advances tens of pixels per frame, so it reveals a couple
of rows and then spends the gesture inside territory it has already visited. A scrollbar
drag sweeps the entire list in one gesture: **every** frame lands in rows nobody has ever
visited, for the whole drag.

`benchScrollbarDrag` models that — a fresh list that has only shown its first screen, then
a 60-frame drag from top to bottom, with the mounted window's measurements fed back through
the batched intake the Fabric integration actually uses, at sizes that differ from the
estimate (a real row almost never measures at exactly the estimate, and that difference is
what forces the suffix to shift). The fixture rebuilds the container for every timed batch:
reusing one would let the first batch pay for revealing the dataset and report every batch
after it as free.

Two costs were linear in list size on every frame:

**Crossing into a fresh row dirtied geometry even when nothing moved.** `layoutElements`
has already given every unmeasured row exactly the fallback size, so a row entering the
window changes only its _state_, not its size — but the measure pass set
`elementsSizeDirty` unconditionally, forcing a full `O(rows)` offset reflow from index zero
on any frame that revealed an unvisited row. Now only a real size change dirties geometry.

**The suffix reflow is the core's hot loop**, and it re-tested the orientation per row and
rewrote `index` on every element even though a size-only reflow never renumbers anything.
The orientation branch is hoisted and the index store is conditional.

### Nothing Phone (2a), µs per drag frame

|    Rows |  Before | After | Change |
| ------: | ------: | ----: | -----: |
|   1,000 |     5.1 |   2.5 |   2.0x |
|  10,000 |    60.9 |  20.4 |   3.0x |
| 100,000 | 1,787.3 | 464.7 |   3.8x |

Fling, cold start, reconcile and every other scenario are unchanged; 45/45 tests still pass.

### What remains, and where the app-level lag actually lives

A drag frame is still `O(rows)` once: when a newly revealed row measures differently from
its estimate, everything after it genuinely shifts, and offsets are eagerly materialized.
Making that logarithmic needs the prefix-sum representation `PERFORMANCE_REPORT.md` lists as
F4's "later option" — a large change to a structure the whole codebase reads directly, and
not obviously worth it: at 10,000 rows the core now costs **20 µs of a 16,700 µs frame**.

That number is the important one. At realistic list sizes the core is no longer a plausible
cause of visible drag lag. What a drag frame still demands is a _complete_ set of new
mounted rows — React render, Yoga measuring fresh multi-paragraph text, and native view
creation — every frame, because the window never overlaps the previous one. That is
precisely what `materializationOverscan` and the commit hook target, and it is where further
work on drag smoothness belongs.

One smaller residual worth noting: `geometryVersion` bumps on every drag frame (the suffix
reflow bumps it), so a list with `snapToItem` enabled rebuilds its `O(rows)` snap table every
drag frame. Harmless for the paged carousels snapping is normally used for, but it would
matter on a long snapping list.

## Limitations

**The Android app could not be rebuilt here.** `repo.maven.apache.org` resolves to `127.0.0.1` in
this environment and Maven Central, npm and GitHub are all unreachable (only Google Maven
resolves), so `com.facebook.react:react-android` and `hermes-android` cannot be fetched and Gradle
cannot configure. End-to-end app verification was therefore done on the iOS Simulator, which builds
fully offline from the vendored Pods. **The C++ core measurements above are still real Nothing
Phone numbers** — the benchmark cross-compiles with the local NDK and runs natively on the device
over adb.

**Neither iPhone could be used directly.** Both paired devices are absent from the provisioning
profile, and adding one requires Apple's servers.

**No app-level frame timings were captured for the "after" state.** `device-scroll.sh` collects
real `gfxinfo framestats` (jank rate, p50/p90/p95/p99 frame times) during scripted flings, but it
needs an Android build of the change. Run it once the Maven dependencies are reachable to get the
user-visible number that complements the core measurements.

**The microbenchmark measures core work only.** It excludes React, Yoga, text/image measurement,
event delivery, JNI, mounting and GPU work. It is the right instrument for the changes made (they
are all in the core and the Fabric shadow node) but it is not a frame-rate claim.

**The warm-update rows repeat the same offset**, so caches and branch prediction are ideal and the
absolute values are optimistic. This is true of the baseline column too, so the comparison holds;
the `FLING` rows, which advance the offset every frame, are the more honest absolute numbers.

## Not done

`F6` (sparse multi-column visibility), `F9` (coalescing native scroll reports), `F11`'s data-sharing
half, `F12` (coordinated overscan limits beyond the direction-aware change), `F13` (Android sticky
handle caching), `F14` (lock-hold reduction), `F15` (refresh latency) and `F16` (drag/keyboard) are
untouched. `F6` and `F9` are architectural; the rest are workload-specific and worth re-evaluating
against fresh device evidence now that per-frame cost no longer scales with list size.
