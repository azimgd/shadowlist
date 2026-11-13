#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace azimgd::shadowlist {

/*
 * The multiplier and the modulus of the avatar hash. A name keeps its palette slot across
 * sessions, platforms and list positions.
 */
constexpr std::uint64_t AVATAR_HASH_MULTIPLIER = 31;
constexpr std::uint64_t AVATAR_HASH_MODULUS = 2147483647;

/*
 * The palette slot of a name: hash = (hash * 31 + unit) % 2147483647 over the name's UTF-16
 * code units, then hash % paletteSize. 0 for an empty palette. Half a surrogate pair is no
 * code point in UTF-8 and hashes as REPLACEMENT_CODE_POINT, on every platform.
 */
std::size_t avatarColorIndex(std::string_view name, std::size_t paletteSize);

/*
 * The first code point of the name's first word and of its last word when it has more than
 * one, as UTF-8. Words split at white space, see isWhiteSpace. The host turns the result to
 * upper case with its own string type, which knows every script.
 */
std::string avatarInitials(std::string_view name);

/*
 * The share of an avatar's size its initials are set at, and the share their line box takes.
 */
constexpr double AVATAR_INITIALS_FONT_SHARE = 0.43;
constexpr double AVATAR_INITIALS_LINE_SHARE = 0.52;

/*
 * How an avatar's initials are typeset, in the units of the avatar's size.
 */
struct AvatarInitialsMetrics {
  double fontSize = 0.0;
  double lineHeight = 0.0;
};

/*
 * The font size and the line box of the initials in an avatar of size. Both are whole numbers:
 * the font size rounds down and the line box rounds up, 17 and 21 for an avatar of 40. The
 * initials keep that size whatever the reader's text size setting.
 */
AvatarInitialsMetrics avatarInitialsMetrics(double size);

}
