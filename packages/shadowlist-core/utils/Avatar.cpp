#include <shadowlist-core/utils/Avatar.hpp>

#include <shadowlist-core/utils/Text.hpp>

#include <cmath>

namespace azimgd::shadowlist {

namespace {

constexpr char32_t ASTRAL_FIRST = 0x10000;
constexpr char32_t HIGH_SURROGATE_FIRST = 0xD800;
constexpr char32_t LOW_SURROGATE_FIRST = 0xDC00;

std::uint64_t mixAvatarHash(std::uint64_t hash, std::uint64_t unit) {
  return (hash * AVATAR_HASH_MULTIPLIER + unit) % AVATAR_HASH_MODULUS;
}

}

std::size_t avatarColorIndex(std::string_view name, std::size_t paletteSize) {
  if (paletteSize == 0) {
    return 0;
  }
  std::uint64_t hash = 0;
  std::size_t offset = 0;
  while (offset < name.size()) {
    char32_t codePoint = decodeUtf8(name, offset);
    if (codePoint < ASTRAL_FIRST) {
      hash = mixAvatarHash(hash, codePoint);
      continue;
    }
    // An astral code point is two UTF-16 units, and each one goes into the hash.
    char32_t astral = codePoint - ASTRAL_FIRST;
    hash = mixAvatarHash(hash, HIGH_SURROGATE_FIRST + (astral >> 10));
    hash = mixAvatarHash(hash, LOW_SURROGATE_FIRST + (astral & 0x3FF));
  }
  return static_cast<std::size_t>(hash % paletteSize);
}

std::string avatarInitials(std::string_view name) {
  char32_t first = 0;
  char32_t last = 0;
  std::size_t wordCount = 0;
  bool inWord = false;
  std::size_t offset = 0;
  while (offset < name.size()) {
    char32_t codePoint = decodeUtf8(name, offset);
    if (isWhiteSpace(codePoint)) {
      inWord = false;
      continue;
    }
    if (inWord) {
      continue;
    }
    inWord = true;
    ++wordCount;
    if (wordCount == 1) {
      first = codePoint;
    } else {
      last = codePoint;
    }
  }
  std::string initials;
  if (wordCount > 0) {
    appendUtf8(initials, first);
  }
  if (wordCount > 1) {
    appendUtf8(initials, last);
  }
  return initials;
}

AvatarInitialsMetrics avatarInitialsMetrics(double size) {
  return {std::floor(size * AVATAR_INITIALS_FONT_SHARE), std::ceil(size * AVATAR_INITIALS_LINE_SHARE)};
}

}
