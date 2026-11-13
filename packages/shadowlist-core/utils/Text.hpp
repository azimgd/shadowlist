#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace azimgd::shadowlist {

/*
 * Text helpers for the template logic in this folder. Text crosses into utils as UTF-8. JS,
 * NSString and Java strings index UTF-16 code units, and a function that counts those says
 * Utf16 in its name.
 */

/*
 * What a byte that starts no valid sequence reads as.
 */
constexpr char32_t REPLACEMENT_CODE_POINT = 0xFFFD;

/*
 * The code point at offset, and offset moved past it. A malformed byte reads as
 * REPLACEMENT_CODE_POINT and moves offset by one. A surrogate pair written as two sequences of
 * three bytes, which JNI's modified UTF-8 does, reads as its one code point.
 */
char32_t decodeUtf8(std::string_view text, std::size_t& offset);

/*
 * Appends codePoint to text as UTF-8.
 */
void appendUtf8(std::string& text, char32_t codePoint);

/*
 * How many UTF-16 code units text takes.
 */
std::size_t lengthUtf16(std::string_view text);

/*
 * Whether codePoint is white space as a JS regular expression's \s and String.trim see it:
 * tab, line feed, vertical tab, form feed, carriage return, space, U+00A0, U+1680, U+2000 to
 * U+200A, U+2028, U+2029, U+202F, U+205F, U+3000 and U+FEFF.
 */
constexpr bool isWhiteSpace(char32_t codePoint) {
  return (codePoint >= 0x09 && codePoint <= 0x0D) || codePoint == 0x20 || codePoint == 0xA0 || codePoint == 0x1680 ||
    (codePoint >= 0x2000 && codePoint <= 0x200A) || codePoint == 0x2028 || codePoint == 0x2029 ||
    codePoint == 0x202F || codePoint == 0x205F || codePoint == 0x3000 || codePoint == 0xFEFF;
}

/*
 * Whether codePoint ends a line for a JS regular expression's dot: line feed, carriage return,
 * U+2028 and U+2029.
 */
constexpr bool isLineTerminator(char32_t codePoint) {
  return codePoint == 0x0A || codePoint == 0x0D || codePoint == 0x2028 || codePoint == 0x2029;
}

/*
 * text without the white space at its start.
 */
std::string_view trimStart(std::string_view text);

/*
 * text without the white space at both ends.
 */
std::string_view trim(std::string_view text);

/*
 * Whether text is empty or white space only.
 */
bool isBlank(std::string_view text);

}
