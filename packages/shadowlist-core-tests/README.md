# shadowlist-core-tests

Integration tests for the platform-agnostic C++ core in
[`../shadowlist-core`](../shadowlist-core). They exercise the same public API
the Fabric layer uses (`Virtualizer::update` for the commit phase,
`Virtualizer::updateElementAtIndex` + `recomputeTotalSize` for the layout phase,
`Container::resolveStateUpdate` for publishing back to the scroll view) and
assert on the resulting layout.

No third-party dependencies — just the core and a small built-in test
framework (`TestFramework.hpp`).

## Run

```sh
cmake -S . -B build
cmake --build build
./build/shadowlist_core_tests
```

Or compile directly (the core is dependency free):

```sh
c++ -std=c++17 -I.. -I. -DSLT_FIXTURES_DIR="\"$PWD/fixtures\"" \
  ../shadowlist-core/*.cpp tests_*.cpp main.cpp -o run_tests -pthread
./run_tests
```

`tests_concurrent.cpp` exercises overlapping `update()` calls on a shared
`Container`. A missing/incorrect lock there is a data race (undefined behaviour),
which a plain run only catches sporadically — build with ThreadSanitizer for a
deterministic check (the real core is race-free):

```sh
c++ -std=c++17 -fsanitize=thread -I.. -I. -DSLT_FIXTURES_DIR="\"$PWD/fixtures\"" \
  ../shadowlist-core/*.cpp tests_*.cpp main.cpp -o run_tests_tsan -pthread
./run_tests_tsan
```

## Layout

| File | Covers |
| --- | --- |
| `Harness.hpp` | `Sim` — drives one render frame (update → measure → publish) and `settle()`s scroll corrections |
| `tests_default.cpp` | top-to-bottom layout, scrolling (exact virtualization window), header/footer |
| `tests_inverted.cpp` | bottom-pinning, short lists, empty → repopulate, estimate-grows re-pin |
| `tests_append_prepend.cpp` | append growth, reconcile shrink, prepend MVCP (fixed + variable heights) |
| `tests_scroll_to_index.cpp` | scrollToIndex (fixed + variable heights, inverted, imperative nonce + declarative prop) |
| `tests_columns.cpp` | multi-column grid layout (uniform + per-track variable heights) |
| `tests_callbacks.cpp` | onEndReached threshold / onStartReached / onScroll / onVisibleIndicesChange dedup |
| `tests_observer.cpp` | `Observer` throttling, pending-dispatch flush, subscribe/unsubscribe |
| `tests_nested.cpp` | independent vertical + horizontal containers driven interleaved |
| `tests_concurrent.cpp` | concurrent `update()` on a shared `Container` (serialized by `coreMutex`) — run under TSan |
| `tests_variable_feed.cpp` | realistic variable-height feed from `fixtures/feed_heights.txt` |
| `tests_perf.cpp` | timing logs (µs/iteration) for `update()`, full `frame()`, and scrollToIndex convergence |

Every test also prints its own wall-clock time, e.g. `[PASS]   0.035 ms  name`, with a
total at the end. `tests_perf.cpp` adds `[perf] …` lines with per-iteration cost; build
with `-O2` for representative release numbers.

## Notes

`Sim` models off-screen rows with the core's frozen estimate (as the real
integration does), so totals/scrollbars for unmeasured rows are approximate by
design; the assertions target properties that hold regardless — visible rows
stacked by their measured size, the scrollToIndex target landing at the viewport
edge, MVCP shifting the offset by the inserted height, etc.

Add a test by dropping a `TEST(name) { ... }` block into any `tests_*.cpp`
(CMake globs them); it self-registers.
