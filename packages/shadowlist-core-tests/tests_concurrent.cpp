// Concurrent invocations: Integration can commit (and therefore run Virtualizer::update)
// from several threads onto the one Container shared by a list's clones. The
// core's coreMutex must serialize those so no revision-conflict exception
// escapes and the container is left consistent.
//
// NOTE: a missing/incorrect lock is a *data race* (undefined behaviour), which
// surfaces as a sporadic crash or silent corruption rather than a catchable C++
// exception, so the assertions below cannot reliably detect it on their own. Build
// this suite with ThreadSanitizer (`-fsanitize=thread`) for a deterministic check
// (the real, locked core is race-free under TSan). To give a race something to
// catch, each thread drives a *different* key set / offset so the threads genuinely
// contend on the shared revision state instead of converging on identical data.

#include "TestFramework.hpp"
#include "Harness.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace slt;

TEST(concurrent_updates_on_shared_container_are_serialized) {
  Container container;
  Virtualizer virtualizer;

  std::atomic<int> exceptions{0};
  std::atomic<int> completed{0};
  const int threadCount = 8;
  const int iterations = 50;

  std::vector<std::thread> pool;
  for (int t = 0; t < threadCount; ++t) {
    pool.emplace_back([&, t]() {
      // Each thread reconciles to a distinct list length and scrolls to a
      // distinct offset, so overlapping updates mutate the shared revision toward
      // conflicting states (a real race, not idempotent repetition).
      for (int i = 0; i < iterations; ++i) {
        try {
          FrameInput input;
          input.keys = makeKeys(100 + t * 25);          // 100..275 elements per thread
          input.containerOffsetY = double((t * 37 + i * 13) % 4000);
          input.windowContainerWidth = 400;
          input.windowContainerHeight = 600;
          input.estimatedElementSize = {400, 100};
          // update() takes container.coreMutex internally, serializing overlaps.
          virtualizer.update(&container, input);
        } catch (...) {
          ++exceptions;
        }
      }
      ++completed;
    });
  }
  for (auto& thread : pool) {
    thread.join();
  }

  CHECK_EQ(exceptions.load(), 0);            // no revision-conflict throw escaped
  CHECK_EQ(completed.load(), threadCount);   // every thread finished

  // The container is left in a consistent, idle state holding one thread's full
  // reconcile (whichever committed last), i.e. a valid per-thread length.
  std::size_t size = container.getElementsSize();
  CHECK((size - 100) % 25 == std::size_t(0) && size >= std::size_t(100) && size <= std::size_t(275));
  CHECK_EQ(container.nextRevisionStatus, std::size_t(0));  // RevisionStatusIdle
}

// Two independent containers driven concurrently never interact (no shared state).
TEST(concurrent_independent_containers) {
  Container a;
  Container b;
  Virtualizer va;
  Virtualizer vb;
  auto keysA = makeKeys(120, "a");
  auto keysB = makeKeys(80, "b");

  std::atomic<int> exceptions{0};
  auto drive = [&](Container& c, Virtualizer& v, const std::vector<std::string>& keys) {
    for (int i = 0; i < 100; ++i) {
      try {
        FrameInput input;
        input.keys = keys;
        input.windowContainerWidth = 400;
        input.windowContainerHeight = 600;
        input.estimatedElementSize = {400, 100};
        v.update(&c, input);
      } catch (...) {
        ++exceptions;
      }
    }
  };

  std::thread ta(drive, std::ref(a), std::ref(va), std::cref(keysA));
  std::thread tb(drive, std::ref(b), std::ref(vb), std::cref(keysB));
  ta.join();
  tb.join();

  CHECK_EQ(exceptions.load(), 0);
  CHECK_EQ(a.getElementsSize(), keysA.size());
  CHECK_EQ(b.getElementsSize(), keysB.size());
}
