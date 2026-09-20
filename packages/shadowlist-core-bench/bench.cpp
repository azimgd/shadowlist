/*
 * ShadowList core benchmark.
 *
 * Exercises the platform-agnostic core through the same call sequence the Fabric
 * integration uses, so a change to the core can be compared before/after on the same
 * host AND on a real device (the binary cross-compiles for Android arm64 and runs
 * under adb; see run.sh).
 *
 * It drives the core the way the Fabric integration does: keys are borrowed through
 * FrameInput::keysRef, scroll frames declare FrameInput::keysUnchanged, and a mounted
 * window's measurements go through applyElementSize plus one commitElementSizes.
 *
 * Output is a machine-readable TSV block plus a human-readable table. Every scenario
 * reports the median of `batches` timed batches; each batch is the mean over
 * `opsPerBatch` operations, so a single slow batch (scheduler, thermal) cannot dominate.
 */

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace azimgd::shadowlist;

namespace {

/* ------------------------------------------------------------------ *
 * Fixture geometry: a 390x840 viewport with 120pt rows and one column.
 * ------------------------------------------------------------------ */
constexpr double WINDOW_WIDTH = 390.0;
constexpr double WINDOW_HEIGHT = 840.0;
constexpr double ROW_HEIGHT = 120.0;
// Card extent along the scroll axis for the horizontal carousel fixture.
constexpr double CARD_WIDTH = 140.0;
constexpr double OVERSCAN = 1.0;

// Rows the viewport plus one viewport of overscan on each side covers.
constexpr std::size_t MOUNTED_ROWS =
  static_cast<std::size_t>((WINDOW_HEIGHT * (1.0 + 2.0 * OVERSCAN)) / ROW_HEIGHT);

struct Timing {
  double medianUs = 0.0;
  double minUs = 0.0;
  double maxUs = 0.0;
};

struct Result {
  std::string scenario;
  std::size_t rows = 0;
  Timing timing;
  // Free-form unit so a scenario can report per-frame instead of per-op.
  const char* unit = "us/op";
};

std::vector<Result> results;

/*
 * Publishes an object's address to a volatile sink, so the optimizer has to keep the work
 * that produced the object instead of eliminating it as a dead store.
 */
const void* volatile escapedObject = nullptr;

template <typename T>
void doNotOptimize(const T& value) {
  escapedObject = &value;
}

double nowUs() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double, std::micro>(clock::now().time_since_epoch()).count();
}

/*
 * Run `body` in batches and keep the median batch mean. `setup` runs untimed before
 * every batch (including warmups) so each batch starts from the same state.
 */
Timing measure(
  std::size_t opsPerBatch,
  std::size_t batches,
  const std::function<void()>& setup,
  const std::function<void(std::size_t)>& body) {
  constexpr std::size_t WARMUP_BATCHES = 3;

  for (std::size_t warmup = 0; warmup < WARMUP_BATCHES; ++warmup) {
    setup();
    for (std::size_t op = 0; op < opsPerBatch; ++op) {
      body(op);
    }
  }

  std::vector<double> batchMeans;
  batchMeans.reserve(batches);
  for (std::size_t batch = 0; batch < batches; ++batch) {
    setup();
    double start = nowUs();
    for (std::size_t op = 0; op < opsPerBatch; ++op) {
      body(op);
    }
    double elapsed = nowUs() - start;
    batchMeans.push_back(elapsed / static_cast<double>(opsPerBatch));
  }

  std::vector<double> sorted = batchMeans;
  std::sort(sorted.begin(), sorted.end());
  Timing timing;
  timing.medianUs = sorted[sorted.size() / 2];
  timing.minUs = sorted.front();
  timing.maxUs = sorted.back();
  return timing;
}

void record(const std::string& scenario, std::size_t rows, Timing timing, const char* unit = "us/op") {
  results.push_back({scenario, rows, timing, unit});
  std::printf("  %-52s %8zu  %12.3f  %s\n", scenario.c_str(), rows, timing.medianUs, unit);
  std::fflush(stdout);
}

/* ------------------------------------------------------------------ *
 * Key fixtures
 * ------------------------------------------------------------------ */
std::vector<std::string> makeShortKeys(std::size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back("k" + std::to_string(index));
  }
  return keys;
}

std::vector<std::string> makeLongKeys(std::size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back("message-0123456789abcdef-0123456789abcdef-" + std::to_string(index));
  }
  return keys;
}

/* ------------------------------------------------------------------ *
 * Frame construction
 * ------------------------------------------------------------------ */

// A frame borrowing `keys`, which must outlive every update the frame is passed to.
FrameInput makeInput(const std::vector<std::string>& keys, double offsetY) {
  FrameInput input;
  input.keysRef = &keys;
  input.containerOffsetY = offsetY;
  input.windowContainerWidth = WINDOW_WIDTH;
  input.windowContainerHeight = WINDOW_HEIGHT;
  input.overscan = OVERSCAN;
  input.columns = 1;
  input.estimatedElementSize = {WINDOW_WIDTH, ROW_HEIGHT};
  return input;
}

/*
 * Bring a container to a fully measured steady state without the O(N^2) cost of
 * measuring one row at a time: stamp geometry directly, then reflow once.
 */
void stampFullyMeasured(Container& container, std::size_t count, double mainSize, bool horizontal = false) {
  double width = horizontal ? mainSize : WINDOW_WIDTH;
  double height = horizontal ? WINDOW_HEIGHT : mainSize;
  for (std::size_t index = 0; index < count; ++index) {
    Element& element = container.revision.elements[index];
    element.width = width;
    element.height = height;
    element.estimated = true;
    element.measured = true;
  }
  container.revision.measuredRealCount = count;
  container.revision.measuredRealTotalWidth = width * static_cast<double>(count);
  container.revision.measuredRealTotalHeight = height * static_cast<double>(count);
  Virtualizer::recomputeElementOffsets(&container, 0);
  Virtualizer::recomputeTotalSize(&container);
}

/*
 * Hand a whole mounted window's measurements to the core the way Fabric's layout pass
 * does: record every size, then reflow once from the lowest row that changed.
 */
template <typename SizeFn>
void feedWindowMeasurements(Container& container, std::size_t low, std::size_t high, SizeFn sizeOf) {
  std::size_t lowestChanged = UNDEFINED_INDEX;
  for (std::size_t index = low; index <= high; ++index) {
    if (Virtualizer::applyElementSize(&container, index, sizeOf(index)) && index < lowestChanged) {
      lowestChanged = index;
    }
  }
  if (lowestChanged != UNDEFINED_INDEX) {
    Virtualizer::commitElementSizes(&container, lowestChanged);
  }
}

/* ------------------------------------------------------------------ *
 * Scenarios
 * ------------------------------------------------------------------ */

/*
 * Warm update, partially estimated: an ordinary scroll frame on a list where only the
 * visited window has been natively measured, the default state of a large list.
 */
void benchWarmUpdatePartial(const std::vector<std::string>& keys, std::size_t rows) {
  Container container;
  Virtualizer::update(&container, makeInput(keys, 0.0));

  // Repeated commits carrying the same key collection, as a scroll settle produces.
  FrameInput input = makeInput(keys, 0.0);
  input.keysUnchanged = true;

  Timing timing = measure(
    25, 7,
    [] {},
    [&](std::size_t) { Virtualizer::update(&container, input); });
  record("warm top update, partially estimated", rows, timing);
}

// Warm update, fully measured: the same frame once every row carries a real measurement.
void benchWarmUpdateMeasured(const std::vector<std::string>& keys, std::size_t rows, double offsetY, const char* label) {
  Container container;
  Virtualizer::update(&container, makeInput(keys, 0.0));
  stampFullyMeasured(container, rows, ROW_HEIGHT);
  Virtualizer::update(&container, makeInput(keys, offsetY));

  FrameInput input = makeInput(keys, offsetY);
  input.keysUnchanged = true;

  Timing timing = measure(
    25, 7,
    [] {},
    [&](std::size_t) { Virtualizer::update(&container, input); });
  record(label, rows, timing);
}

/*
 * Unchanged measurements: Fabric feeds every mounted row's measured size back during
 * layout. These rows already have that exact size, so the ideal cost is zero reflow.
 * `startIndex` picks where in the list the mounted window sits.
 */
void benchUnchangedMeasurements(const std::vector<std::string>& keys, std::size_t rows, std::size_t startIndex, const char* label) {
  Container container;
  Virtualizer::update(&container, makeInput(keys, 0.0));
  stampFullyMeasured(container, rows, ROW_HEIGHT);

  std::size_t count = std::min<std::size_t>(20, rows - startIndex);
  Timing timing = measure(
    5, 7,
    [] {},
    [&](std::size_t) {
      for (std::size_t offset = 0; offset < count; ++offset) {
        Virtualizer::updateElementAtIndex(&container, startIndex + offset, {WINDOW_WIDTH, ROW_HEIGHT});
      }
    });
  record(label, rows, timing, "us/20-row batch");
}

/*
 * Key copy: the cost of copying the whole key collection into a fresh FrameInput, which
 * borrowing through keysRef avoids.
 */
void benchKeyCopy(const std::vector<std::string>& keys, std::size_t rows, const char* label, std::size_t opsPerBatch) {
  Timing timing = measure(
    opsPerBatch, 7,
    [] {},
    [&](std::size_t) {
      FrameInput input;
      input.keys = keys;
      doNotOptimize(input.keys);
    });
  record(label, rows, timing);
}

// Snap offsets: snap geometry is read on every publishing layout pass.
void benchSnapOffsets(const std::vector<std::string>& keys, std::size_t rows) {
  Container container;
  FrameInput input = makeInput(keys, 0.0);
  input.snapToItem = true;
  Virtualizer::update(&container, input);
  stampFullyMeasured(container, rows, ROW_HEIGHT);

  Timing timing = measure(
    25, 7,
    [] {},
    [&](std::size_t) {
      const auto& offsets = container.getSnapOffsets();
      doNotOptimize(offsets);
    });
  record("generate snap offsets", rows, timing);
}

// Reconcile: one append plus one removal, the shape a paginating or chat list produces.
void benchReconcile(const std::vector<std::string>& keys, std::size_t rows) {
  Container container;
  Virtualizer::update(&container, makeInput(keys, 0.0));
  stampFullyMeasured(container, rows, ROW_HEIGHT);

  std::vector<std::string> grown = keys;
  grown.push_back("k-appended");

  Timing timing = measure(
    25, 7,
    [] {},
    [&](std::size_t op) {
      Virtualizer::reconcileElements(&container, (op % 2 == 0) ? grown : keys);
    });
  record("reconcile alternating append-one/remove-one", rows, timing);
}

/*
 * Ahead-of-time predictions arriving for rows DEEP in the list.
 *
 * This is the shape the size prediction pipeline produces: the host measures a window a
 * screen or two ahead of what is mounted and stages the sizes, so the rows that change are
 * nowhere near the front of the list. A reflow that always restarts from row 0 gets this
 * case badly wrong, and no other scenario in this file exercises it.
 *
 * Rows are deliberately left unmeasured: a natively measured row outranks a prediction,
 * so stamping them first would make every staged size a no-op.
 */
void benchDeepPredictions(const std::vector<std::string>& keys, std::size_t rows) {
  if (rows < 400) {
    return;
  }

  Container container;
  Virtualizer::update(&container, makeInput(keys, 0.0));

  std::size_t base = rows - 200;

  Timing timing = measure(
    10, 7,
    [] {},
    [&](std::size_t op) {
      for (std::size_t offset = 0; offset < 20; ++offset) {
        container.setPredictedSize(
          keys[base + offset],
          {WINDOW_WIDTH, ROW_HEIGHT + static_cast<double>((op + offset) % 11)});
      }
      FrameInput input = makeInput(keys, 0.0);
      input.keysUnchanged = true;
      Virtualizer::update(&container, input);
    });
  record("predictions land deep in the list", rows, timing, "us/frame");
}

// Cold start: the first update on a fresh container builds every Element.
void benchColdUpdate(const std::vector<std::string>& keys, std::size_t rows) {
  std::vector<double> samples;
  samples.reserve(7);
  for (std::size_t sample = 0; sample < 7 + 3; ++sample) {
    Container container;
    FrameInput input = makeInput(keys, 0.0);
    double start = nowUs();
    Virtualizer::update(&container, input);
    double elapsed = nowUs() - start;
    if (sample >= 3) {
      samples.push_back(elapsed);
    }
  }
  std::sort(samples.begin(), samples.end());
  Timing timing{samples[samples.size() / 2], samples.front(), samples.back()};
  record("cold first update", rows, timing);
}

// Element construction: per-row cost of a default Element (metadata only; debug ids are lazy).
void benchElementConstruction(std::size_t rows) {
  Timing timing = measure(
    5, 7,
    [] {},
    [&](std::size_t) {
      std::vector<Element> elements(rows);
      doNotOptimize(elements);
    });
  record("construct and destroy N default Elements", rows, timing);
}

/* ------------------------------------------------------------------ *
 * End-to-end frame loops. These are the numbers a user actually feels: one
 * iteration is one display frame's worth of core work during a fling, including
 * the size feedback Fabric performs for every mounted row.
 * ------------------------------------------------------------------ */

struct FlingOptions {
  std::size_t columns = 1;
  bool horizontal = false;
  bool inverted = false;
  // Pixels advanced per simulated frame. ~2600 px/s at 60fps is a hard fling.
  double pixelsPerFrame = 44.0;
  double startOffset = 0.0;
  const char* label = "fling";
};

/*
 * One simulated frame: publish the new scroll offset, then feed each mounted row's
 * measured size back one row at a time, as ShadowListViewShadowNode::replaceChild does.
 */
void benchFling(const std::vector<std::string>& keys, std::size_t rows, const FlingOptions& options) {
  Container container;

  /*
   * A scroll frame carries the same keys as the one before it, which is exactly what
   * Fabric can prove from props identity. `first` skips that claim for the priming call.
   */
  auto frameInput = [&](double offset, bool first) {
    FrameInput input = makeInput(keys, 0.0);
    input.columns = options.columns;
    input.horizontal = options.horizontal;
    input.inverted = options.inverted;
    input.userScrolled = true;
    input.scrollPhase = ScrollPhase::Dragging;
    input.keysUnchanged = !first;
    if (options.horizontal) {
      input.containerOffsetX = offset;
      input.estimatedElementSize = {CARD_WIDTH, WINDOW_HEIGHT};
    } else {
      input.containerOffsetY = offset;
    }
    return input;
  };

  Virtualizer::update(&container, frameInput(options.startOffset, true));
  stampFullyMeasured(container, rows, options.horizontal ? CARD_WIDTH : ROW_HEIGHT, options.horizontal);

  /*
   * How many rows this scenario actually mounted and re-measured per frame, summed over
   * the run. Reported alongside the timing so a suspiciously fast result can be told
   * apart from a scenario that quietly stopped doing anything.
   */
  std::size_t rowsTouched = 0;
  std::size_t framesRun = 0;

  double offset = options.startOffset;
  Timing timing = measure(
    30, 7,
    [&] { offset = options.startOffset; },
    [&](std::size_t) {
      offset += options.pixelsPerFrame;
      ++framesRun;
      Virtualizer::update(&container, frameInput(offset, false));

      /*
       * Fabric's layout pass then hands every mounted row its measured size back. In a
       * steady fling those sizes are unchanged for all but the newly revealed rows.
       */
      auto visible = container.getVisibleIndices();
      if (visible.first == UNDEFINED_INDEX) {
        return;
      }
      std::size_t low = std::min(visible.first, visible.second);
      std::size_t high = std::max(visible.first, visible.second);
      high = std::min(high, low + MOUNTED_ROWS * options.columns);
      for (std::size_t index = low; index <= high && index < rows; ++index) {
        /*
         * A multi-column row's cross-axis extent belongs to the track layout, not to
         * measurement, so the integration keeps the core's value for that axis and feeds
         * back only the scroll-axis measurement. Reporting a full-width measurement into
         * a 3-track layout would just fight the track sizing every frame.
         */
        const Element& element = container.getElementAtIndex(index);
        Size measured = options.horizontal
          ? Size{CARD_WIDTH, WINDOW_HEIGHT}
          : Size{WINDOW_WIDTH, ROW_HEIGHT};
        if (options.columns > 1) {
          if (options.horizontal) {
            measured.height = element.height;
          } else {
            measured.width = element.width;
          }
        }
        Virtualizer::updateElementAtIndex(&container, index, measured);
        ++rowsTouched;
      }
    });

  /*
   * A fling frame must mount a window's worth of rows. If it did not, the timing above is
   * measuring nothing and must not be reported as a speedup.
   */
  std::size_t expectedMounted = options.horizontal
    ? static_cast<std::size_t>((WINDOW_WIDTH * (1.0 + 2.0 * OVERSCAN)) / CARD_WIDTH)
    : MOUNTED_ROWS;
  if (framesRun == 0 || rowsTouched / framesRun < expectedMounted / 2) {
    std::printf("  !! %s did no meaningful work (%zu rows over %zu frames)\n",
      options.label, rowsTouched, framesRun);
  }
  record(options.label, rows, timing, "us/frame");
}

/*
 * Dragging the scroll indicator.
 *
 * This is a different workload from a fling, and a much harsher one. A fling advances by
 * tens of pixels per frame, so it reveals a couple of new rows and then spends the rest of
 * the gesture inside territory it has already visited. Dragging the scrollbar sweeps the
 * whole list in one gesture: every frame teleports into rows nobody has looked at yet,
 * for the entire drag.
 *
 * Modelled faithfully: a fresh list that has only shown its first screen, then a 60-frame
 * drag of the indicator from top to bottom, feeding back the mounted window's sizes each
 * frame the way Fabric's layout pass does.
 */
void benchScrollbarDrag(const std::vector<std::string>& keys, std::size_t rows, const char* label) {
  constexpr std::size_t DRAG_FRAMES = 60;

  /*
   * Every timed batch must be a first drag over a list that has only shown its first
   * screen. Reusing one container would mean the first batch pays for revealing the whole
   * dataset and every later batch sweeps rows that are already visited, which is the cheap
   * case and would report the drag as free. The container is therefore rebuilt in setup,
   * which measure() runs untimed.
   */
  std::unique_ptr<Container> container;
  double step = 1.0;
  std::size_t rowsTouched = 0;
  std::size_t framesRun = 0;
  double offset = 0.0;

  auto reset = [&] {
    container = std::make_unique<Container>();
    Virtualizer::update(container.get(), makeInput(keys, 0.0));
    auto firstWindow = container->getVisibleIndices();
    if (firstWindow.first != UNDEFINED_INDEX) {
      for (std::size_t index = firstWindow.first; index <= firstWindow.second && index < rows; ++index) {
        Virtualizer::updateElementAtIndex(container.get(), index, {WINDOW_WIDTH, ROW_HEIGHT});
      }
    }
    double total = container->revision.totalContainerHeight;
    step = total > WINDOW_HEIGHT ? (total - WINDOW_HEIGHT) / DRAG_FRAMES : 1.0;
    offset = 0.0;
  };

  Timing timing = measure(
    DRAG_FRAMES, 7,
    reset,
    [&](std::size_t) {
      offset += step;
      ++framesRun;

      FrameInput input = makeInput(keys, offset);
      input.userScrolled = true;
      input.scrollPhase = ScrollPhase::Dragging;
      input.keysUnchanged = true;
      Virtualizer::update(container.get(), input);

      auto visible = container->getVisibleIndices();
      if (visible.first == UNDEFINED_INDEX) {
        return;
      }
      std::size_t low = std::min(visible.first, visible.second);
      std::size_t high = std::min(std::max(visible.first, visible.second), low + MOUNTED_ROWS);
      if (high >= rows) {
        high = rows - 1;
      }
      /*
       * A real row almost never measures at exactly the estimate, and the difference is
       * what forces the suffix to shift. Feeding back the estimate verbatim would make
       * this the easy case rather than the one users actually hit.
       */
      feedWindowMeasurements(*container, low, high, [](std::size_t index) {
        return Size{WINDOW_WIDTH, ROW_HEIGHT + static_cast<double>(index % 9) * 3.0};
      });
      rowsTouched += high - low + 1;
    });

  if (framesRun == 0 || rowsTouched / framesRun < MOUNTED_ROWS / 2) {
    std::printf("  !! %s did no meaningful work (%zu rows over %zu frames)\n",
      label, rowsTouched, framesRun);
  }
  record(label, rows, timing, "us/frame");
}

/*
 * Inverted list, "tap the status bar to jump to the top": the offset moves from the
 * resting bottom all the way to 0 in a single frame, then settles.
 */
void benchInvertedScrollToTop(const std::vector<std::string>& keys, std::size_t rows) {
  Container container;

  bool primed = false;
  auto frameInput = [&](double offset, bool userScrolled) {
    FrameInput input = makeInput(keys, offset);
    input.inverted = true;
    input.userScrolled = userScrolled;
    if (userScrolled) {
      input.scrollPhase = ScrollPhase::Dragging;
    }
    input.keysUnchanged = primed;
    return input;
  };

  Virtualizer::update(&container, frameInput(0.0, false));
  primed = true;
  stampFullyMeasured(container, rows, ROW_HEIGHT);
  double bottom = container.revision.totalContainerHeight - WINDOW_HEIGHT;
  Virtualizer::update(&container, frameInput(bottom, true));

  Timing timing = measure(
    10, 7,
    [&] { Virtualizer::update(&container, frameInput(bottom, true)); },
    [&](std::size_t) {
      // The jump plus the settle frames the scroll view reports on the way.
      Virtualizer::update(&container, frameInput(0.0, true));
      Virtualizer::update(&container, frameInput(0.0, false));
    });
  record("inverted list: jump to top (tap status bar)", rows, timing, "us/jump");
}

/*
 * Chat prepend: a page of older messages arrives while the user sits mid-list. The core
 * has to reconcile, reflow and hold the anchored row in place.
 */
void benchPrependWhileScrolled(const std::vector<std::string>& keys, std::size_t rows) {
  Container container;
  Virtualizer::update(&container, makeInput(keys, 0.0));
  stampFullyMeasured(container, rows, ROW_HEIGHT);

  std::vector<std::string> prepended;
  prepended.reserve(rows + 30);
  for (std::size_t index = 0; index < 30; ++index) {
    prepended.push_back("older-" + std::to_string(index));
  }
  prepended.insert(prepended.end(), keys.begin(), keys.end());

  double offset = ROW_HEIGHT * 40.0;
  Timing timing = measure(
    10, 7,
    [] {},
    [&](std::size_t op) {
      FrameInput input = makeInput((op % 2 == 0) ? prepended : keys, offset);
      Virtualizer::update(&container, input);
    });
  record("prepend 30 rows while scrolled (chat)", rows, timing, "us/prepend");
}

/*
 * scrollToIndex into unmeasured territory, centred in the viewport: the correction stays in
 * flight for several frames while the region around the target is measured, rederiving its
 * resting place on each of them (see resolveAnchorSubOffset).
 */
void benchScrollToIndexCentred(const std::vector<std::string>& keys, std::size_t rows) {
  const std::size_t target = rows * 3 / 5;
  Container container;
  Virtualizer::update(&container, makeInput(keys, 0.0));
  // Only the opening window is laid out; everything past it is still on its estimate.
  feedWindowMeasurements(container, 0, MOUNTED_ROWS, [](std::size_t) {
    return Size{WINDOW_WIDTH, ROW_HEIGHT};
  });

  std::uint64_t sequence = 0;
  Timing timing = measure(
    4, 7,
    [] {},
    [&](std::size_t) {
      container.requestScrollToIndex(
        static_cast<double>(target), static_cast<double>(++sequence), -2, 0.5);
      // The settle: each frame mounts and measures the window the correction moved onto.
      for (int frame = 0; frame < 6; ++frame) {
        Virtualizer::update(&container, makeInput(keys, container.revision.containerOffsetY));
        auto visible = container.getVisibleIndices();
        if (visible.first != UNDEFINED_INDEX) {
          std::size_t low = std::min(visible.first, visible.second);
          std::size_t high = std::min(std::max(visible.first, visible.second), rows - 1);
          feedWindowMeasurements(container, low, high, [](std::size_t index) {
            // Rows nothing like the estimate, so the resting place actually has to move.
            return Size{WINDOW_WIDTH, index % 3 == 0 ? ROW_HEIGHT * 3.0 : ROW_HEIGHT};
          });
        }
      }
      doNotOptimize(container.revision);
    });
  record("scrollToIndex centred, target unmeasured", rows, timing, "us/jump");
}

/* ------------------------------------------------------------------ *
 * Reporting
 * ------------------------------------------------------------------ */
void emitMachineReadable() {
  std::printf("\n#BEGIN_TSV\nscenario\trows\tmedian_us\tmin_us\tmax_us\tunit\n");
  for (const Result& result : results) {
    std::printf("%s\t%zu\t%.3f\t%.3f\t%.3f\t%s\n",
      result.scenario.c_str(), result.rows,
      result.timing.medianUs, result.timing.minUs, result.timing.maxUs, result.unit);
  }
  std::printf("#END_TSV\n");
}

}

int main(int argc, char** argv) {
  // Default dataset sizes: small, medium and large lists.
  std::vector<std::size_t> sizes = {1000, 10000, 100000};
  if (argc > 1 && std::strcmp(argv[1], "--quick") == 0) {
    sizes = {10000};
  }

  std::printf("ShadowList core benchmark\n");
  std::printf("%-54s %8s  %12s  %s\n", "scenario", "rows", "median", "unit");
  std::printf("---------------------------------------------------------------------------------------\n");

  for (std::size_t rows : sizes) {
    std::vector<std::string> shortKeys = makeShortKeys(rows);
    std::vector<std::string> longKeys = makeLongKeys(rows);

    benchWarmUpdatePartial(shortKeys, rows);
    benchWarmUpdateMeasured(shortKeys, rows, 0.0, "warm top update, fully measured");
    benchWarmUpdateMeasured(shortKeys, rows, static_cast<double>(rows - 20) * ROW_HEIGHT, "warm deep update, fully measured");

    benchUnchangedMeasurements(shortKeys, rows, 0, "20 unchanged measurements at start");
    benchUnchangedMeasurements(shortKeys, rows, rows / 2, "20 unchanged measurements at midpoint");
    benchUnchangedMeasurements(shortKeys, rows, rows - 20, "20 unchanged measurements in final 20 rows");

    benchDeepPredictions(shortKeys, rows);

    benchKeyCopy(shortKeys, rows, "copy short keys into new FrameInput", 25);
    benchKeyCopy(longKeys, rows, "copy long keys into new FrameInput", 10);

    benchSnapOffsets(shortKeys, rows);
    benchReconcile(shortKeys, rows);
    benchColdUpdate(shortKeys, rows);
    benchElementConstruction(rows);

    FlingOptions vertical;
    vertical.label = "FLING vertical single column";
    benchFling(shortKeys, rows, vertical);

    FlingOptions deep;
    deep.label = "FLING vertical, deep in list";
    deep.startOffset = static_cast<double>(rows) * ROW_HEIGHT * 0.75;
    benchFling(shortKeys, rows, deep);

    FlingOptions horizontal;
    horizontal.horizontal = true;
    horizontal.label = "FLING horizontal";
    benchFling(shortKeys, rows, horizontal);

    FlingOptions grid;
    grid.columns = 3;
    grid.label = "FLING 3-column grid";
    benchFling(shortKeys, rows, grid);

    FlingOptions invertedFling;
    invertedFling.inverted = true;
    invertedFling.label = "FLING inverted (chat)";
    benchFling(shortKeys, rows, invertedFling);

    benchScrollbarDrag(shortKeys, rows, "DRAG scrollbar top to bottom");
    benchInvertedScrollToTop(shortKeys, rows);
    benchPrependWhileScrolled(shortKeys, rows);
    benchScrollToIndexCentred(shortKeys, rows);

    std::printf("---------------------------------------------------------------------------------------\n");
  }

  emitMachineReadable();
  return 0;
}
