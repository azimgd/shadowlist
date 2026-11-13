/*
 * Avatar tests: the palette slot a name hashes to, the initials it shows and the size they are
 * set at.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/utils/Avatar.hpp>

#include <cmath>
#include <cstdint>
#include <string>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(an_avatar_color_hashes_utf16_units) {
  // "Ab": (65 * 31 + 98) % 2147483647 = 2113.
  CHECK_EQ(avatarColorIndex("Ab", 1000), std::size_t(113));
  CHECK_EQ(avatarColorIndex("Ab", 7), std::size_t(2113 % 7));
  // U+00E9 is one unit of 233, not its two bytes.
  CHECK_EQ(avatarColorIndex("\xC3\xA9", 1000), std::size_t(233));
}

TEST(an_avatar_color_hashes_both_units_of_an_astral_code_point) {
  // U+1F600 is 0xD83D then 0xDE00: (55357 * 31 + 56832) % 2147483647 = 1772899.
  CHECK_EQ(avatarColorIndex("\xF0\x9F\x98\x80", 10000000), std::size_t(1772899));
}

TEST(an_avatar_color_hashes_half_a_surrogate_pair_as_the_replacement) {
  // 0xD83D alone, then "A": (65533 * 31 + 65) % 2147483647 = 2031588.
  CHECK_EQ(avatarColorIndex("\xED\xA0\xBD" "A", 1000), std::size_t(588));
  CHECK_EQ(avatarColorIndex("\xEF\xBF\xBD" "A", 1000), std::size_t(588));
  CHECK_EQ(avatarInitials("\xED\xA0\xBD" "A"), std::string("\xEF\xBF\xBD"));
}

TEST(an_avatar_color_stays_inside_the_palette) {
  std::string name;
  for (int length = 0; length < 200; ++length) {
    name.push_back(static_cast<char>('a' + length % 26));
    CHECK(avatarColorIndex(name, 8) < 8);
    CHECK_EQ(avatarColorIndex(name, 1), std::size_t(0));
  }
}

TEST(an_avatar_color_of_an_empty_name_or_palette_is_the_first) {
  CHECK_EQ(avatarColorIndex("", 8), std::size_t(0));
  CHECK_EQ(avatarColorIndex("Ada", 0), std::size_t(0));
}

TEST(an_avatar_color_depends_on_the_order_of_the_letters) {
  CHECK(avatarColorIndex("Ada Lovelace", 1000003) != avatarColorIndex("Lovelace Ada", 1000003));
}

TEST(initials_take_the_first_and_last_word) {
  CHECK_EQ(avatarInitials("Ada Lovelace"), std::string("AL"));
  CHECK_EQ(avatarInitials("Mary Jane Watson"), std::string("MW"));
  CHECK_EQ(avatarInitials("Plato"), std::string("P"));
  CHECK_EQ(avatarInitials(""), std::string(""));
  CHECK_EQ(avatarInitials("   "), std::string(""));
}

TEST(initials_keep_the_case_of_the_name) {
  CHECK_EQ(avatarInitials("ada lovelace"), std::string("al"));
}

TEST(initials_split_at_any_white_space) {
  // A tab, a line feed, a run of spaces and a no-break space each split words.
  CHECK_EQ(avatarInitials("Alan\tTuring"), std::string("AT"));
  CHECK_EQ(avatarInitials("Alan\nTuring"), std::string("AT"));
  CHECK_EQ(avatarInitials("  Grace   Hopper  "), std::string("GH"));
  CHECK_EQ(avatarInitials("Jean\xC2\xA0Luc"), std::string("JL"));
}

TEST(initials_keep_an_astral_code_point_whole) {
  // U+1F600 takes two UTF-16 units and stays one initial.
  CHECK_EQ(avatarInitials("\xF0\x9F\x98\x80 Smile"), std::string("\xF0\x9F\x98\x80S"));
}

TEST(initials_leave_the_middle_words_out) {
  CHECK_EQ(avatarInitials("Gabriel Garcia Marquez"), std::string("GM"));
}

TEST(avatar_initials_are_set_at_whole_numbers_of_the_avatar_size) {
  AvatarInitialsMetrics metrics = avatarInitialsMetrics(40.0);
  CHECK_EQ(metrics.fontSize, 17.0);
  CHECK_EQ(metrics.lineHeight, 21.0);
  // 100 * 0.43 is a whole number, and so is 100 * 0.52.
  CHECK_EQ(avatarInitialsMetrics(100.0).fontSize, 43.0);
  CHECK_EQ(avatarInitialsMetrics(100.0).lineHeight, 52.0);
  CHECK_EQ(avatarInitialsMetrics(0.0).fontSize, 0.0);
  CHECK_EQ(avatarInitialsMetrics(0.0).lineHeight, 0.0);
}

TEST(avatar_initials_fit_their_line_box) {
  for (double size = 1.0; size <= 200.0; size += 0.5) {
    AvatarInitialsMetrics metrics = avatarInitialsMetrics(size);
    CHECK(metrics.fontSize <= metrics.lineHeight);
    CHECK(metrics.lineHeight <= std::ceil(size));
  }
}
