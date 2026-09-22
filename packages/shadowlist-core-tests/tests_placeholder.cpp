/*
 * Keeps the test binary building while the suite is rewritten.
 * CMake picks up every tests_ file, so real tests can replace this one.
 */

#include "TestFramework.hpp"

using namespace slt;

TEST(placeholder) {
  CHECK(true);
}
