#include "TestFramework.hpp"
#include <shadowlist-core/host/ListDriver.hpp>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(list_driver_sticky_overlay_uses_its_measured_extent_for_the_shared_push_off) {
  ListDriver driver;
  driver.setKeys({"a", "b", "c", "d", "e", "f"});
  driver.setMeasureRow([](std::size_t, const std::string&, double) { return 100.0; });
  driver.setStickyIndices({0, 3});
  PassInput input;
  input.windowAlong = 200.0;
  input.windowCross = 300.0;
  driver.runPasses(input);
  CHECK_EQ(driver.activeStickyIndex(250.0), 0u);
  CHECK_EQ(driver.stickyLeading(0, 250.0), 200.0);
  CHECK_EQ(driver.stickyLeading(0, 250.0, 40.0), 250.0);
  CHECK_EQ(driver.stickyLeading(0, 250.0, 120.0), 180.0);
  CHECK_EQ(driver.stickyLeading(3, 350.0, 40.0), 350.0);
}
