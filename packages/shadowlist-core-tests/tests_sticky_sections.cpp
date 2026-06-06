// Sticky section headers: rows tagged via stickyIndices. resolveStickyHeader
// reports which header is pinned at the current offset and how far to translate it
// from its resting position - the active header pins to the viewport start and is
// pushed up by the next section header as it scrolls in.

#include "TestFramework.hpp"
#include "Harness.hpp"

using namespace slt;
using azimgd::shadowlist::StickyHeader;

// Configure a list whose section headers sit at the given indices, rows uniform.
static void makeSectioned(Sim& sim, const std::vector<std::size_t>& headers, std::size_t count, double rowH = 100.0) {
  sim.winH = 600;
  sim.estimated = {400, rowH};
  sim.sizeOfKey = [rowH](const std::string&) { return Size{400, rowH}; };
  sim.stickyIndices = headers;
  sim.setKeys(makeKeys(count));
  sim.settle();
}

// At rest the first section header is the active one, sitting at its resting
// position (translation 0).
TEST(sticky_section_first_header_active_at_top) {
  Sim sim;
  makeSectioned(sim, {0, 5, 10}, 30);

  StickyHeader sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)0);
  CHECK_NEAR(sticky.translation, 0.0, 0.5);
}

// Scrolled into the first section, header 0 stays active and is translated down by
// the scroll offset so it pins to the viewport start.
TEST(sticky_section_active_header_tracks_offset) {
  Sim sim;
  makeSectioned(sim, {0, 5, 10}, 30);
  sim.scrollTo(250.0);

  StickyHeader sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)0);
  CHECK_NEAR(sticky.translation, 250.0, 0.5);
}

// As the next section header (index 5, offset 500) approaches, it pushes the
// active header up: the active header's displayed top is pinned to nextTop - size
// rather than the viewport start.
TEST(sticky_section_next_header_pushes_active_up) {
  Sim sim;
  makeSectioned(sim, {0, 5, 10}, 30);
  sim.scrollTo(450.0);

  // offset 450, next header top 500, active size 100 -> displayedTop = 400,
  // translation = 400 - 0.
  StickyHeader sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)0);
  CHECK_NEAR(sticky.translation, 400.0, 0.5);
}

// Once scrolled to the next section header's resting offset it becomes the active
// header, pinned at the viewport start (translation 0).
TEST(sticky_section_swaps_to_next_header) {
  Sim sim;
  makeSectioned(sim, {0, 5, 10}, 30);
  sim.scrollTo(500.0);

  StickyHeader sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)5);
  CHECK_NEAR(sticky.translation, 0.0, 0.5);

  sim.scrollTo(560.0);
  sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)5);
  CHECK_NEAR(sticky.translation, 60.0, 0.5);
}

// When the first section header rests below the scroll offset (e.g. a non-sticky
// list header occupies the start) nothing is pinned yet.
TEST(sticky_section_none_before_first_header) {
  Sim sim;
  makeSectioned(sim, {2, 6, 10}, 30);
  sim.scrollTo(100.0); // first sticky header rests at offset 200

  StickyHeader sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, UNDEFINED_INDEX);
  CHECK_NEAR(sticky.translation, 0.0, 0.5);
}

// No sticky indices configured (a plain list) never pins anything.
TEST(sticky_section_empty_indices_never_pins) {
  Sim sim;
  makeSectioned(sim, {}, 30);
  sim.scrollTo(800.0);

  StickyHeader sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, UNDEFINED_INDEX);
}

// Variable-height sections: section headers (40) are shorter than items (100).
// The geometry follows the real measured offsets, not a uniform stride.
TEST(sticky_section_variable_heights) {
  Sim sim;
  sim.winH = 600;
  sim.estimated = {400, 100};
  // Section layout: header(40) + 4 items(100) per section.
  // offsets: h0=0, items 40/140/240/340, h1=440, items 480/580/680/780, h2=880 ...
  auto isHeader = [](const std::string& key) {
    // headers are k0, k5, k10, ...
    int n = std::stoi(key.substr(1));
    return n % 5 == 0;
  };
  sim.sizeOfKey = [isHeader](const std::string& key) {
    return isHeader(key) ? Size{400, 40} : Size{400, 100};
  };
  sim.stickyIndices = {0, 5, 10};
  sim.setKeys(makeKeys(30));
  sim.settle();

  // Within the first section, header 0 pins to the viewport start.
  sim.scrollTo(200.0);
  StickyHeader sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)0);
  CHECK_NEAR(sticky.translation, 200.0, 0.5);

  // Next header rests at 440; at offset 420 it pushes header 0 up so its bottom
  // meets the next header's top: displayedTop = 440 - 40 = 400, translation 400.
  sim.scrollTo(420.0);
  sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)0);
  CHECK_NEAR(sticky.translation, 400.0, 0.5);

  // At the next header's resting offset it takes over with translation 0.
  sim.scrollTo(440.0);
  sticky = sim.container.resolveStickyHeader();
  CHECK_EQ(sticky.index, (std::size_t)5);
  CHECK_NEAR(sticky.translation, 0.0, 0.5);
}
