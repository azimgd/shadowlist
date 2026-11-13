/*
 * Text helper tests: reading and writing UTF-8, and the white space JS sees.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/utils/Text.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

std::vector<char32_t> codePointsOf(std::string_view text) {
  std::vector<char32_t> codePoints;
  std::size_t offset = 0;
  while (offset < text.size()) {
    codePoints.push_back(decodeUtf8(text, offset));
  }
  return codePoints;
}

}

TEST(utf8_decodes_sequences_of_every_length) {
  std::vector<char32_t> expected = {U'a', 0xE9, 0x65E5, 0x1F600};
  CHECK(codePointsOf("a\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80") == expected);
}

TEST(utf8_round_trips_through_append) {
  for (char32_t codePoint : {char32_t(0), char32_t(0x7F), char32_t(0x80), char32_t(0x7FF), char32_t(0x800),
         char32_t(0xFFFF), char32_t(0x10000), char32_t(0x10FFFF)}) {
    std::string text;
    appendUtf8(text, codePoint);
    std::size_t offset = 0;
    CHECK(decodeUtf8(text, offset) == codePoint);
    CHECK_EQ(offset, text.size());
  }
}

TEST(a_malformed_byte_reads_as_the_replacement_and_moves_one_byte) {
  // A continuation byte alone, a lead byte cut short, a sequence longer than it needs, a byte past the last lead.
  for (std::string_view text : {"\x80z", "\xE6\x97z", "\xC0\xAFz", "\xFFz", "\xF4\x90\x80\x80z"}) {
    std::size_t offset = 0;
    CHECK(decodeUtf8(text, offset) == REPLACEMENT_CODE_POINT);
    CHECK_EQ(offset, std::size_t(1));
    CHECK(codePointsOf(text).back() == U'z');
  }
}

TEST(a_surrogate_pair_in_two_sequences_reads_as_one_code_point) {
  // U+1F600 as JNI's modified UTF-8 writes it.
  std::string_view pair = "\xED\xA0\xBD\xED\xB8\x80";
  std::size_t offset = 0;
  CHECK(decodeUtf8(pair, offset) == char32_t(0x1F600));
  CHECK_EQ(offset, pair.size());
  // Half a pair is no code point.
  CHECK(codePointsOf("\xED\xA0\xBDz") == (std::vector<char32_t>{REPLACEMENT_CODE_POINT, U'z'}));
  CHECK(codePointsOf("\xED\xB8\x80") == (std::vector<char32_t>{REPLACEMENT_CODE_POINT}));
}

TEST(appending_a_surrogate_writes_the_replacement) {
  std::string text;
  appendUtf8(text, 0xD800);
  appendUtf8(text, 0x110000);
  CHECK_EQ(text, std::string("\xEF\xBF\xBD\xEF\xBF\xBD"));
}

TEST(utf16_length_counts_an_astral_code_point_twice) {
  CHECK_EQ(lengthUtf16(""), std::size_t(0));
  CHECK_EQ(lengthUtf16("abc"), std::size_t(3));
  CHECK_EQ(lengthUtf16("\xC3\xA9\xE6\x97\xA5"), std::size_t(2));
  CHECK_EQ(lengthUtf16("a\xF0\x9F\x98\x80"), std::size_t(3));
}

TEST(white_space_is_the_set_js_matches) {
  for (char32_t codePoint : {char32_t(0x09), char32_t(0x0A), char32_t(0x0B), char32_t(0x0C), char32_t(0x0D),
         char32_t(0x20), char32_t(0xA0), char32_t(0x1680), char32_t(0x2000), char32_t(0x200A), char32_t(0x2028),
         char32_t(0x2029), char32_t(0x202F), char32_t(0x205F), char32_t(0x3000), char32_t(0xFEFF)}) {
    CHECK(isWhiteSpace(codePoint));
  }
  // A next line mark, a zero width space and a Mongolian vowel separator are not.
  for (char32_t codePoint : {char32_t(0x08), char32_t(0x0E), char32_t(0x85), char32_t(0x200B), char32_t(0x180E),
         char32_t(0x2060), U'a'}) {
    CHECK(!isWhiteSpace(codePoint));
  }
}

TEST(line_terminators_are_the_four_a_js_dot_stops_at) {
  for (char32_t codePoint : {char32_t(0x0A), char32_t(0x0D), char32_t(0x2028), char32_t(0x2029)}) {
    CHECK(isLineTerminator(codePoint));
  }
  CHECK(!isLineTerminator(0x0B));
  CHECK(!isLineTerminator(0x85));
  CHECK(!isLineTerminator(U' '));
}

TEST(trim_drops_white_space_of_any_width_at_both_ends) {
  CHECK(trim("  a b  ") == "a b");
  CHECK(trim("\t\n a\r\n") == "a");
  // A no-break space before and an ideographic space after.
  CHECK(trim("\xC2\xA0" "caf\xC3\xA9" "\xE3\x80\x80") == "caf\xC3\xA9");
  CHECK(trim("") == "");
  CHECK(trim(" \xC2\xA0 ") == "");
  CHECK(trimStart("  a  ") == "a  ");
  CHECK(trim("a") == "a");
}

TEST(a_blank_text_is_empty_or_white_space_only) {
  CHECK(isBlank(""));
  CHECK(isBlank(" \t\xC2\xA0"));
  CHECK(!isBlank(" . "));
}
