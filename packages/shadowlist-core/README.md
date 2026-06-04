# shadowlist-core

Shared C++ virtualization engine used by `shadowlist` and `shadowlist-wasm`.

`shadowlist-core` does not render UI on its own. It resolves list layout and scroll state for an integration that owns the actual views.

## What It Does

- Reconciles keys across updates
- Virtualizes the visible window plus buffer
- Handles variable-size items
- Supports inverted lists
- Supports horizontal lists
- Supports multi-column layouts
- Keeps visible content anchored during prepend / append / remeasure
- Resolves `scrollToIndex`
- Computes visible and viewable index ranges
- Supports sticky header and sticky footer offsets

## Main Types

- `Container`
  Stores current revision state, callbacks, measurements, and scroll state.

- `Virtualizer`
  Runs the frame update and layout logic.

- `FrameInput`
  Input payload for a single frame: keys, offsets, viewport size, list flags, thresholds, and estimates.

## How It Is Usually Used

An integration typically does this on each frame:

1. Build a `FrameInput`
2. Call `virtualizer.update(&container, input)`
3. Measure mounted items in the host platform
4. Feed those sizes back with `Virtualizer::updateElementAtIndex`
5. Call `Virtualizer::recomputeTotalSize`
6. Publish the result from `container.resolveStateUpdate(...)`

## Minimal Example

```cpp
#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

using namespace azimgd::shadowlist;

Container container;
Virtualizer virtualizer;

FrameInput input;
input.keys = {"a", "b", "c"};
input.windowContainerWidth = 400;
input.windowContainerHeight = 700;
input.estimatedElementSize = {400, 120};

virtualizer.update(&container, input);

Virtualizer::updateElementAtIndex(&container, 0, Size{400, 80});
Virtualizer::updateElementAtIndex(&container, 1, Size{400, 140});
Virtualizer::recomputeTotalSize(&container);

auto state = container.resolveStateUpdate(
  0.0,
  0.0,
  0.0,
  0.0);
```

## Scripts And Commands

There is no package-level `package.json` here. The main supported commands live in `shadowlist-core-tests` and `shadowlist-wasm`.

### Run Core Tests With CMake

```sh
cmake -S packages/shadowlist-core-tests -B build/shadowlist-core-tests
cmake --build build/shadowlist-core-tests
./build/shadowlist-core-tests/shadowlist_core_tests
```

### Run Core Tests Without CMake

```sh
cd packages/shadowlist-core-tests
c++ -std=c++17 -I.. -I. -DSLT_FIXTURES_DIR="\"$PWD/fixtures\"" \
  ../shadowlist-core/*.cpp tests_*.cpp main.cpp -o run_tests -pthread
./run_tests
```

### Run ThreadSanitizer

```sh
cd packages/shadowlist-core-tests
c++ -std=c++17 -fsanitize=thread -I.. -I. -DSLT_FIXTURES_DIR="\"$PWD/fixtures\"" \
  ../shadowlist-core/*.cpp tests_*.cpp main.cpp -o run_tests_tsan -pthread
./run_tests_tsan
```

### Rebuild The WASM Artifacts

```sh
cd packages/shadowlist-wasm
yarn build:wasm
```

## Testing

See [`../shadowlist-core-tests/README.md`](../shadowlist-core-tests/README.md) for the full test breakdown, perf coverage, and fixture notes.
