// Performance logs: how long the core's hot paths take per iteration.
// These print timing lines (and always pass); they are micro-benchmarks, not
// assertions on absolute speed (which would be machine-dependent).

#include "TestFramework.hpp"
#include "Harness.hpp"

#include <chrono>
#include <cstdio>
#include <string>

using namespace slt;

namespace {

double msSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(
           std::chrono::steady_clock::now() - start).count();
}

void logPerIter(const char* label, int iters, std::size_t rows, double totalMs, const char* unit) {
  std::printf("       [perf] %-28s x%-5d on %6zu rows : %9.3f ms total, %8.2f %s\n",
              label, iters, rows, totalMs, totalMs * 1000.0 / iters, unit);
}

}  // namespace

// Commit phase only: Virtualizer::update (reconcile + measure + scroll resolve).
TEST(perf_update_commit_phase) {
  const std::size_t rows = 10000;
  Container container;
  Virtualizer virtualizer;
  auto keys = makeKeys(rows);

  FrameInput input;
  input.keys = keys;
  input.windowContainerWidth = 400;
  input.windowContainerHeight = 700;
  input.estimatedElementSize = {400, 120};
  virtualizer.update(&container, input);  // warm up (first revision)

  const int iters = 300;
  const double maxOffset = double(rows) * 120.0 - 700.0;
  auto started = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) {
    input.containerOffsetY = std::fmod(double(i) * 5779.0, maxOffset);  // jump around
    virtualizer.update(&container, input);
  }
  logPerIter("update() commit-phase", iters, rows, msSince(started), "us/iter");
  CHECK(iters > 0);
}

// Full frame: update + per-row measurement + recomputeTotalSize + publish,
// scrolling through a variable-height list.
TEST(perf_frame_full_scrollthrough) {
  const std::size_t rows = 5000;
  Sim sim;
  sim.winH = 700;
  sim.estimated = {400, 120};
  sim.sizeOfKey = [](const std::string& key) {
    std::size_t n = std::stoul(key.substr(1));
    return Size{400, 60.0 + double((n * 37) % 200)};
  };
  sim.setKeys(makeKeys(rows));
  sim.settle();

  const int steps = 200;
  const double totalH = sim.totalAxis();
  auto started = std::chrono::steady_clock::now();
  for (int i = 0; i < steps; ++i) {
    sim.offsetY = totalH * double(i) / double(steps);
    sim.frame();
  }
  logPerIter("frame() full pass", steps, rows, msSince(started), "us/iter");
  CHECK(steps > 0);
}

// Convergence: cost of a whole scrollToIndex "run" (settle to the target).
TEST(perf_scroll_to_index_convergence) {
  const std::size_t rows = 5000;
  Sim sim;
  sim.winH = 700;
  sim.estimated = {400, 120};
  sim.sizeOfKey = [](const std::string& key) {
    std::size_t n = std::stoul(key.substr(1));
    return Size{400, 60.0 + double((n * 37) % 200)};
  };
  sim.setKeys(makeKeys(rows));
  sim.settle();

  const int jumps = 50;
  auto started = std::chrono::steady_clock::now();
  for (int i = 0; i < jumps; ++i) {
    sim.container.scrollToIndex(std::size_t(i * 97) % rows);
    sim.settle();
  }
  double total = msSince(started);
  std::printf("       [perf] %-28s x%-5d on %6zu rows : %9.3f ms total, %8.2f ms/jump\n",
              "scrollToIndex + settle", jumps, rows, total, total / jumps);
  CHECK(jumps > 0);
}
