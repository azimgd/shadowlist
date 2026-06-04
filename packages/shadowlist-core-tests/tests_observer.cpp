// Observer: the throttled callback wrapper a few integrations subscribe to. It
// dispatches on revision end when the visible range / measured size changed, but
// no more than once per throttle window; a change that lands inside the window is
// held as a pending dispatch and flushed on the next revision (or via flush()).

#include "TestFramework.hpp"
#include "Harness.hpp"

#include <shadowlist-core/Observer.hpp>

#include <string>

using namespace slt;
using azimgd::shadowlist::Observer;

// Drive a frame that changes the visible window (new keys / new offset) so the
// observer sees a real change to dispatch.
static void commitFrame(azimgd::shadowlist::Container& container,
                        azimgd::shadowlist::Virtualizer& virtualizer,
                        std::size_t count, double offsetY) {
  FrameInput input;
  input.keys = makeKeys(count);
  input.containerOffsetY = offsetY;
  input.windowContainerWidth = 400;
  input.windowContainerHeight = 600;
  input.estimatedElementSize = {400, 100};
  virtualizer.update(&container, input);
}

// A large throttle window: the first revision dispatches immediately (no prior
// dispatch), the next change is held pending, and flush() delivers it. After
// unsubscribe no further callbacks run.
TEST(observer_throttles_and_flushes_pending) {
  azimgd::shadowlist::Container container;
  azimgd::shadowlist::Virtualizer virtualizer;
  Observer observer(container, /*throttleMs*/ 100000);

  int fired = 0;
  std::size_t subscription = observer.subscribe([&](azimgd::shadowlist::Container&) { ++fired; });
  container.setObserver(&observer);

  commitFrame(container, virtualizer, 50, 0.0);
  CHECK_EQ(fired, 1);  // first dispatch is immediate

  commitFrame(container, virtualizer, 50, 1500.0);
  CHECK_EQ(fired, 1);  // within the throttle window -> held pending, not delivered

  observer.flush();
  CHECK_EQ(fired, 2);  // pending dispatch flushed

  observer.unsubscribe(subscription);
  commitFrame(container, virtualizer, 50, 3000.0);
  observer.flush();
  CHECK_EQ(fired, 2);  // unsubscribed: no further callbacks
}

// A zero throttle window dispatches on every revision that actually changed.
TEST(observer_zero_throttle_dispatches_each_change) {
  azimgd::shadowlist::Container container;
  azimgd::shadowlist::Virtualizer virtualizer;
  Observer observer(container, /*throttleMs*/ 0);

  int fired = 0;
  observer.subscribe([&](azimgd::shadowlist::Container&) { ++fired; });
  container.setObserver(&observer);

  commitFrame(container, virtualizer, 50, 0.0);
  commitFrame(container, virtualizer, 50, 1500.0);  // window changed
  commitFrame(container, virtualizer, 50, 3000.0);  // window changed
  CHECK(fired >= 3);
}
