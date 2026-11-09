/*
 * Java strings to and from standard UTF-8, the encoding the key store holds keys in. Strings cross
 * as UTF-16 because the JNI's own UTF-8 is modified UTF-8, which writes a character past U+FFFF as
 * two 3 byte surrogates and would never match the key JSI stored. A lone surrogate becomes U+FFFD,
 * as JSI writes it.
 */

#pragma once

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

inline void appendUtf8(std::string& out, std::uint32_t point) {
  if (point < 0x80) {
    out.push_back(static_cast<char>(point));
  } else if (point < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (point >> 6)));
    out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
  } else if (point < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (point >> 12)));
    out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (point >> 18)));
    out.push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
  }
}

inline std::string stringFromJava(JNIEnv* env, jstring value) {
  if (value == nullptr) {
    return {};
  }
  const jsize length = env->GetStringLength(value);
  if (length == 0) {
    return {};
  }
  std::vector<jchar> units(static_cast<std::size_t>(length));
  env->GetStringRegion(value, 0, length, units.data());
  std::string result;
  result.reserve(units.size());
  for (std::size_t index = 0; index < units.size(); ++index) {
    std::uint32_t point = units[index];
    if (point >= 0xD800 && point <= 0xDBFF && index + 1 < units.size() && units[index + 1] >= 0xDC00 &&
        units[index + 1] <= 0xDFFF) {
      point = 0x10000 + ((point - 0xD800) << 10) + (units[index + 1] - 0xDC00);
      index += 1;
    } else if (point >= 0xD800 && point <= 0xDFFF) {
      point = 0xFFFD;
    }
    appendUtf8(result, point);
  }
  return result;
}

/*
 * A malformed sequence becomes U+FFFD, one per byte that cannot start or continue a character.
 */
inline jstring stringToJava(JNIEnv* env, const std::string& value) {
  std::vector<jchar> units;
  units.reserve(value.size());
  std::size_t index = 0;
  while (index < value.size()) {
    const auto lead = static_cast<unsigned char>(value[index]);
    std::size_t length = 0;
    std::uint32_t point = 0;
    std::uint32_t least = 0;
    if (lead < 0x80) {
      length = 1;
      point = lead;
    } else if (lead >= 0xC2 && lead <= 0xDF) {
      length = 2;
      point = lead & 0x1F;
      least = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      length = 3;
      point = lead & 0x0F;
      least = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      length = 4;
      point = lead & 0x07;
      least = 0x10000;
    }
    bool valid = length > 0 && index + length <= value.size();
    for (std::size_t next = 1; valid && next < length; ++next) {
      const auto unit = static_cast<unsigned char>(value[index + next]);
      valid = (unit & 0xC0) == 0x80;
      point = (point << 6) | (unit & 0x3F);
    }
    valid = valid && point >= least && point <= 0x10FFFF && (point < 0xD800 || point > 0xDFFF);
    if (!valid) {
      units.push_back(0xFFFD);
      index += 1;
      continue;
    }
    index += length;
    if (point >= 0x10000) {
      point -= 0x10000;
      units.push_back(static_cast<jchar>(0xD800 + (point >> 10)));
      units.push_back(static_cast<jchar>(0xDC00 + (point & 0x3FF)));
    } else {
      units.push_back(static_cast<jchar>(point));
    }
  }
  const jchar none = 0;
  return env->NewString(units.empty() ? &none : units.data(), static_cast<jsize>(units.size()));
}

}
