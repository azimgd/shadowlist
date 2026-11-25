#include "TestFramework.hpp"
#include <shadowlist-core/host/ListDriver.hpp>
#include <cmath>
#include <limits>
#include <string>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {
struct Host {
  ListDriver driver;
  double offset = 0.0;
  Host() {
    std::vector<std::string> keys;
    for (std::size_t index = 0; index < 20; ++index) {
      keys.push_back("k" + std::to_string(index));
    }
    driver.setKeys(std::move(keys));
    driver.setMeasureRow([](std::size_t, const std::string&, double) { return 100.0; });
    layout();
  }
  void layout() {
    PassInput input;
    input.offset = offset;
    input.windowAlong = 100.0;
    input.windowCross = 200.0;
    input.userScrolled = false;
    offset = driver.runPasses(input).offset;
  }
  auto update(double now) { return driver.updateViewability(offset, 100.0, now); }
};
}

TEST(native_viewability_configs_use_actual_geometry_and_percentage_precedence) {
  Host host;
  ViewabilityConfig item;
  item.itemVisiblePercentThreshold = 50.0;
  ViewabilityConfig area;
  area.itemVisiblePercentThreshold = 0.0;
  area.viewAreaCoveragePercentThreshold = 75.0;
  host.driver.setViewabilityConfigs({item, area});
  host.offset = 50.0;
  host.layout();
  auto first = host.update(0.0);
  CHECK_EQ(first.size(), 1u);
  CHECK_EQ(first[0].first, 0u);
  CHECK_EQ(first[0].second.viewable.size(), 2u);
  host.offset = 75.0;
  host.layout();
  auto next = host.update(1.0);
  CHECK_EQ(next.size(), 2u);
  CHECK_EQ(next[0].second.changes.removed[0], std::string("k0"));
  CHECK_EQ(next[1].first, 1u);
  CHECK_EQ(next[1].second.changes.added[0], std::string("k1"));
  CHECK(host.update(2.0).empty());
}

TEST(native_viewability_timing_ignores_programmatic_scroll_and_resets_on_detach) {
  Host host;
  ViewabilityConfig config;
  config.minimumViewTime = 100.0;
  config.waitForInteraction = true;
  host.driver.setViewabilityConfigs({config});
  CHECK(host.update(0.0).empty());
  host.offset = 100.0;
  host.layout();
  CHECK(host.update(50.0).empty());
  CHECK(!host.driver.isViewabilityWaiting());
  host.driver.recordInteraction(60.0);
  CHECK(host.update(159.0).empty());
  auto report = host.update(160.0);
  CHECK_EQ(report.size(), 1u);
  CHECK_EQ(report[0].second.changes.added[0], std::string("k1"));
  CHECK(host.driver.setViewabilityConfigs({config}, 170.0).empty());
  CHECK(host.update(300.0).empty());
  host.driver.resetViewability();
  CHECK(host.update(500.0).empty());
  CHECK(!host.driver.isViewabilityWaiting());
}

TEST(native_viewability_changed_config_cancels_its_prior_wait) {
  Host host;
  ViewabilityConfig config;
  config.minimumViewTime = 100.0;
  host.driver.setViewabilityConfigs({config});
  CHECK(host.update(0.0).empty());
  config.minimumViewTime = 200.0;
  auto changed = host.driver.setViewabilityConfigs({config}, 50.0);
  CHECK_EQ(changed.size(), 1u);
  CHECK(host.update(50.0).empty());
  CHECK(host.update(100.0).empty());
  CHECK(host.update(249.0).empty());
  CHECK_EQ(host.update(250.0).size(), 1u);
  host.driver.setViewabilityConfigs({});
  CHECK(!host.driver.isViewabilityWaiting());
  CHECK(host.update(1000.0).empty());
}

TEST(native_viewability_invalid_threshold_values_match_fabric_rules) {
  ViewabilityConfig config;
  config.itemVisiblePercentThreshold = -50.0;
  CHECK_EQ(viewableRuleFor(config).threshold, 0.0);
  config.itemVisiblePercentThreshold = 150.0;
  CHECK_EQ(viewableRuleFor(config).threshold, 1.0);
  config.viewAreaCoveragePercentThreshold = std::numeric_limits<double>::quiet_NaN();
  auto rule = viewableRuleFor(config);
  CHECK(rule.coverage);
  CHECK(std::isnan(rule.threshold));
  CHECK(isRowViewable(0.0, 50.0, 0.0, 100.0, rule));
  CHECK(!isRowViewable(0.0, 200.0, 0.0, 100.0, rule));
  CHECK(config == config);
}

TEST(native_viewability_masonry_keeps_qualifying_rows_before_a_nonqualifying_gap) {
  ListDriver driver;
  ListSettings settings;
  settings.numberOfColumns = 2;
  driver.setSettings(settings);
  const std::vector<double> heights{100.0, 60.0, 50.0, 100.0, 100.0, 100.0};
  driver.setKeys({"k0", "k1", "k2", "k3", "k4", "k5"});
  driver.setMeasureRow([&heights](std::size_t index, const std::string&, double) { return heights[index]; });
  PassInput input;
  input.windowAlong = 100.0;
  input.windowCross = 200.0;
  driver.runPasses(input);
  input.offset = 50.0;
  driver.runPasses(input);
  CHECK_EQ(driver.getRowRect(0).y, 0.0);
  CHECK_EQ(driver.getRowRect(0).height, 100.0);
  CHECK_EQ(driver.getRowRect(1).y, 0.0);
  CHECK_EQ(driver.getRowRect(1).height, 60.0);
  CHECK_EQ(driver.getRowRect(2).y, 100.0);
  CHECK_EQ(driver.getRowRect(2).height, 50.0);
  ViewabilityConfig config;
  config.itemVisiblePercentThreshold = 50.0;
  driver.setViewabilityConfigs({config});
  auto reports = driver.updateViewability(50.0, 100.0, 0.0);
  CHECK_EQ(reports.size(), 1u);
  CHECK_EQ(reports[0].second.viewable.size(), 3u);
  CHECK_EQ(reports[0].second.viewable[0], std::string("k0"));
  CHECK_EQ(reports[0].second.viewable[1], std::string("k2"));
  CHECK_EQ(reports[0].second.viewable[2], std::string("k3"));
}
