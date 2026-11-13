#include <shadowlist-core/utils/Text.hpp>

namespace azimgd::shadowlist {

namespace {

constexpr char32_t HIGH_SURROGATE_FIRST = 0xD800;
constexpr char32_t LOW_SURROGATE_FIRST = 0xDC00;
constexpr char32_t SURROGATE_LAST = 0xDFFF;
constexpr char32_t ASTRAL_FIRST = 0x10000;
constexpr char32_t CODE_POINT_LAST = 0x10FFFF;

bool isContinuation(unsigned char byte) {
  return (byte & 0xC0) == 0x80;
}

/*
 * One sequence at offset, a surrogate half included, or REPLACEMENT_CODE_POINT with a length
 * of one byte.
 */
char32_t decodeSequence(std::string_view text, std::size_t offset, std::size_t& length) {
  unsigned char lead = static_cast<unsigned char>(text[offset]);
  length = 1;
  if (lead < 0x80) {
    return lead;
  }
  std::size_t count = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
  if (lead < 0xC2 || lead > 0xF4 || offset + count > text.size()) {
    return REPLACEMENT_CODE_POINT;
  }
  char32_t codePoint = lead & (count == 4 ? 0x07 : count == 3 ? 0x0F : 0x1F);
  for (std::size_t index = 1; index < count; ++index) {
    unsigned char byte = static_cast<unsigned char>(text[offset + index]);
    if (!isContinuation(byte)) {
      return REPLACEMENT_CODE_POINT;
    }
    codePoint = (codePoint << 6) | (byte & 0x3F);
  }
  // A code point written longer than it needs, or past the last one, is malformed.
  char32_t smallest = count == 4 ? ASTRAL_FIRST : count == 3 ? 0x800 : 0x80;
  if (codePoint < smallest || codePoint > CODE_POINT_LAST) {
    return REPLACEMENT_CODE_POINT;
  }
  length = count;
  return codePoint;
}

}

char32_t decodeUtf8(std::string_view text, std::size_t& offset) {
  std::size_t length = 0;
  char32_t codePoint = decodeSequence(text, offset, length);
  offset += length;
  if (codePoint < HIGH_SURROGATE_FIRST || codePoint > SURROGATE_LAST) {
    return codePoint;
  }
  if (codePoint < LOW_SURROGATE_FIRST && offset < text.size()) {
    std::size_t lowLength = 0;
    char32_t low = decodeSequence(text, offset, lowLength);
    if (low >= LOW_SURROGATE_FIRST && low <= SURROGATE_LAST) {
      offset += lowLength;
      return ASTRAL_FIRST + ((codePoint - HIGH_SURROGATE_FIRST) << 10) + (low - LOW_SURROGATE_FIRST);
    }
  }
  // Half a pair is no code point.
  return REPLACEMENT_CODE_POINT;
}

void appendUtf8(std::string& text, char32_t codePoint) {
  if ((codePoint >= HIGH_SURROGATE_FIRST && codePoint <= SURROGATE_LAST) || codePoint > CODE_POINT_LAST) {
    codePoint = REPLACEMENT_CODE_POINT;
  }
  if (codePoint < 0x80) {
    text.push_back(static_cast<char>(codePoint));
  } else if (codePoint < 0x800) {
    text.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
    text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else if (codePoint < ASTRAL_FIRST) {
    text.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else {
    text.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  }
}

std::size_t lengthUtf16(std::string_view text) {
  std::size_t length = 0;
  std::size_t offset = 0;
  while (offset < text.size()) {
    length += decodeUtf8(text, offset) >= ASTRAL_FIRST ? 2 : 1;
  }
  return length;
}

std::string_view trimStart(std::string_view text) {
  std::size_t offset = 0;
  while (offset < text.size()) {
    std::size_t next = offset;
    if (!isWhiteSpace(decodeUtf8(text, next))) {
      break;
    }
    offset = next;
  }
  return text.substr(offset);
}

std::string_view trim(std::string_view text) {
  text = trimStart(text);
  std::size_t end = 0;
  std::size_t offset = 0;
  while (offset < text.size()) {
    if (!isWhiteSpace(decodeUtf8(text, offset))) {
      end = offset;
    }
  }
  return text.substr(0, end);
}

bool isBlank(std::string_view text) {
  return trimStart(text).empty();
}

}
