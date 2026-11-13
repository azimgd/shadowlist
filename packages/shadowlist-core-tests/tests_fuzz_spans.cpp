#include "FuzzHelpers.hpp"
#include <shadowlist-core/host/ListDriver.hpp>
#include <numeric>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {
struct SpanItem { std::string key; std::size_t span; double extent; };
struct SpanRect { double along, cross, extent, crossExtent; };

// Independent packing oracle: no TrackLayout or canonical column lookup.
std::vector<SpanRect> packSpans(const std::vector<SpanItem>& items, const std::vector<double>& extents,
  std::size_t columns, double cross, double header) {
  std::vector<double> ends(columns, header);
  std::vector<SpanRect> result;
  std::size_t cursor = 0;
  for (std::size_t index = 0; index < items.size(); ++index) {
    const auto span = std::max<std::size_t>(1, std::min(columns, items[index].span));
    if (cursor + span > columns) cursor = 0;
    const double leading = *std::max_element(ends.begin() + cursor, ends.begin() + cursor + span);
    result.push_back({leading, cross * cursor / columns, extents[index], cross * span / columns});
    std::fill(ends.begin() + cursor, ends.begin() + cursor + span, leading + extents[index]);
    cursor = (cursor + span) % columns;
  }
  return result;
}
}

TEST(fuzz_spans_keyed_edits_measurements_ranges_anchors_and_drag_match_independent_packing) {
  const FuzzKinds kinds{{"append", "prepend", "delete", "reorder", "span", "columns", "cross", "measure", "drag", "scroll", "clamp"}, {}};
  forEachFuzzSeed(__func__, 80, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    Trail trail(seed, episode);
    std::mt19937 rng(seed);
    std::vector<SpanItem> items;
    std::size_t serial = 0;
    auto item = [&] { return SpanItem{"s" + std::to_string(serial++), rng() % 7 + 1, static_cast<double>(rng() % 100 + 12)}; };
    for (int index = 0; index < 24; ++index) items.push_back(item());
    ListDriver driver;
    ListSettings settings;
    settings.numberOfColumns = rng() % 4 + 2;
    settings.horizontal = seed % 2 == 0;
    settings.estimatedRowSize = 48;
    driver.setSettings(settings);
    PassInput input;
    driver.setMeasureRow([&](std::size_t index, const std::string& key, double cross) {
      if (index >= items.size() || key != items[index].key) trail.failWith("measurement resolved stale key/index");
      const auto span = std::max<std::size_t>(1, std::min(settings.numberOfColumns, items[index].span));
      const double expectedCross = input.windowCross * span / settings.numberOfColumns;
      if (std::abs(cross - expectedCross) > 1e-8) trail.failWith("measurement used stale span width");
      return items[index].extent;
    });
    input.windowAlong = 160;
    input.windowCross = 360;
    input.headerSize = rng() % 20;
    input.footerSize = rng() % 20;
    input.clampStart = input.clampEnd = true;
    auto sync = [&] {
      std::vector<std::string> keys;
      std::vector<std::size_t> spans;
      for (const auto& value : items) { keys.push_back(value.key); spans.push_back(value.span); }
      if (keys != driver.getKeys()) driver.setKeys(keys);
      driver.setRowSpans(spans);
    };
    PassResult result;
    auto settle = [&] {
      input.phase = ScrollPhase::Idle;
      input.userScrolled = false;
      for (int frame = 0; frame < FUZZ_MAX_SETTLE_FRAMES; ++frame) {
        result = driver.runPasses(input);
        input.offset = result.offset;
        if (!result.settling) return;
      }
      trail.failWith("layout did not settle");
    };
    auto check = [&] {
      std::vector<double> extents;
      for (std::size_t index = 0; index < items.size(); ++index) extents.push_back(driver.getExtentAt(index));
      const auto expected = packSpans(items, extents, settings.numberOfColumns, input.windowCross, input.headerSize);
      double end = input.headerSize;
      const auto mounted = driver.planMount(input.offset, input.windowAlong, 0);
      for (std::size_t index = 0; index < items.size(); ++index) {
        auto actual = driver.getRowRect(index);
        double along = settings.horizontal ? actual.x : actual.y;
        double cross = settings.horizontal ? actual.y : actual.x;
        double crossExtent = settings.horizontal ? actual.height : actual.width;
        if (std::abs(along - expected[index].along) > 1e-8 || std::abs(cross - expected[index].cross) > 1e-8 ||
            std::abs(crossExtent - expected[index].crossExtent) > 1e-8) trail.failWith("row differs from independent pack at " + std::to_string(index));
        end = std::max(end, along + extents[index]);
        if (along + extents[index] > input.offset && along < input.offset + input.windowAlong) {
          if (mounted.low == UNDEFINED_INDEX || index < mounted.low || index > mounted.high) trail.failWith("overlapping row omitted from mount range");
          if (extents[index] != items[index].extent) trail.failWith("visible row retains estimated size");
        }
        episode.observe(along);
        episode.observe(cross);
      }
      if (std::abs(result.contentAlong - end - input.footerSize) > 1e-8) trail.failWith("content misses an earlier track end");
      if (input.offset < -1e-8 || input.offset > std::max(0.0, result.contentAlong - input.windowAlong) + 1e-8) trail.failWith("resting offset not clamped");
      episode.observe(input.offset);
      return extents;
    };
    sync();
    settle();
    for (int step = 0; step < 22; ++step) {
      const auto action = step % 11;
      const auto index = static_cast<std::size_t>(rng() % items.size());
      std::vector<AnchorState> anchors;
      for (std::size_t row = 0; row < items.size(); ++row) {
        const double leading = driver.getLeadingAt(row);
        if (leading + driver.getExtentAt(row) > input.offset && leading < input.offset + input.windowAlong)
          anchors.push_back({items[row].key, input.offset - leading});
      }
      bool hold = action < 8;
      trail.note("step " + std::to_string(step) + " action " + std::to_string(action) + " index " + std::to_string(index));
      switch (action) {
        case 0: episode.reach("append"); items.push_back(item()); break;
        case 1: episode.reach("prepend"); items.insert(items.begin(), item()); break;
        case 2: episode.reach("delete"); items.erase(items.begin() + index); break;
        case 3: episode.reach("reorder"); std::swap(items[index], items[rng() % items.size()]); break;
        case 4: episode.reach("span"); items[index].span = rng() % 9; break;
        case 5: episode.reach("columns"); settings.numberOfColumns = rng() % 4 + 2; driver.setSettings(settings); break;
        case 6: episode.reach("cross"); input.windowCross = rng() % 300 + 180; break;
        case 7: episode.reach("measure"); items[index].extent = rng() % 300 + 1; driver.markRemeasure({index}); break;
        case 8: episode.reach("drag"); break;
        case 9: episode.reach("scroll"); input.offset = (rng() % 1000) * std::max(0.0, result.contentAlong - input.windowAlong) / 1000;
          input.phase = ScrollPhase::Dragging; input.userScrolled = true; result = driver.runPasses(input); input.offset = result.offset; break;
        case 10: episode.reach("clamp"); input.offset = result.contentAlong; break;
      }
      sync();
      settle();
      const auto extents = check();
      if (hold && !anchors.empty()) {
        // Grid tracks keep the existing on-screen anchor, which need not be the first
        // row returned by the public save-position helper. One original visible key
        // must retain its screen position, or reach the exact legal clamp.
        bool preserved = false;
        bool removed = false;
        for (const auto& anchor : anchors) {
          auto current = driver.indexOfKey(anchor.key);
          if (current == UNDEFINED_INDEX) { removed = true; continue; }
          double wanted = driver.getLeadingAt(current) + anchor.offset;
          double max = std::max(0.0, result.contentAlong - input.windowAlong);
          if (std::abs(input.offset - wanted) <= FUZZ_TOLERANCE || isHoldClamped(wanted, input.offset, max)) preserved = true;
        }
        if (!preserved && !removed) trail.failWith("no original visible anchor retained its screen position");
      }
      if (action == 8) {
        const auto held = static_cast<std::size_t>(rng() % items.size());
        const auto target = static_cast<std::size_t>(rng() % items.size());
        auto source = driver.getRowRect(held);
        auto destination = driver.getRowRect(target);
        const double touch = settings.horizontal ? source.x + source.width / 2 : source.y + source.height / 2;
        const double touchCross = settings.horizontal ? source.y + source.height / 2 : source.x + source.width / 2;
        driver.dragBegin(held, touch, touchCross);
        driver.placeHeld(held, settings.horizontal ? destination.x + destination.width / 2 : destination.y + destination.height / 2,
          settings.horizontal ? destination.y + destination.height / 2 : destination.x + destination.width / 2);
        std::vector<std::size_t> mounted(items.size());
        std::iota(mounted.begin(), mounted.end(), 0);
        driver.dragUpdateInsertion(mounted);
        const auto insertion = driver.getDragInsertionIndex();
        if (insertion != UNDEFINED_INDEX) {
          auto movedItems = items;
          auto movedExtents = extents;
          auto moved = movedItems[held];
          double extent = movedExtents[held];
          movedItems.erase(movedItems.begin() + held);
          movedExtents.erase(movedExtents.begin() + held);
          movedItems.insert(movedItems.begin() + insertion, moved);
          movedExtents.insert(movedExtents.begin() + insertion, extent);
          const auto packed = packSpans(movedItems, movedExtents, settings.numberOfColumns, input.windowCross, input.headerSize);
          for (std::size_t position = 0; position < movedItems.size(); ++position) {
            const auto original = driver.indexOfKey(movedItems[position].key);
            if (original == held) continue;
            const auto resting = driver.getRowRect(original);
            const auto shift = driver.dragShiftFor(original);
            if (std::abs((settings.horizontal ? resting.x : resting.y) + shift.leading - packed[position].along) > 1e-8 ||
                std::abs((settings.horizontal ? resting.y : resting.x) + shift.cross - packed[position].cross) > 1e-8) trail.failWith("drag preview differs from independent pack");
          }
        }
        driver.dragEnd();
      }
    }
  });
}
