/*
 * JNI side of com.shadowlist.utils.ShadowListUtilsCore. Every call is a stateless hop into the
 * template logic of the core's utils folder. Arrays carry the values of a struct in and out.
 */

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <shadowlist-core/utils/Avatar.hpp>
#include <shadowlist-core/utils/BoardColumnLayout.hpp>
#include <shadowlist-core/utils/ChatLayout.hpp>
#include <shadowlist-core/utils/ContactSection.hpp>
#include <shadowlist-core/utils/FeedImageFrame.hpp>
#include <shadowlist-core/utils/PollTally.hpp>
#include <shadowlist-core/utils/RelativeTime.hpp>
#include <shadowlist-core/utils/Text.hpp>

namespace {

namespace sl = azimgd::shadowlist;

/*
 * Slots of ShadowListUtilsCore.constants, CONSTANT_* in Kotlin: template constants of the core in dp.
 */
enum ConstantSlot {
  CONSTANT_CHAT_BUBBLE_PADDING_HORIZONTAL = 0,
  CONSTANT_CHAT_BUBBLE_WIDTH_FRACTION,
  CONSTANT_FEED_SINGLE_IMAGE_HEIGHT,
  CONSTANT_SLOTS,
};

/*
 * Bits of chatMessageSizeSpec's shapeFlags, the same as ShadowListUtilsCore.CHAT_SHAPE_*.
 */
enum ChatShapeFlag {
  CHAT_SHAPE_HAS_TEXT = 1 << 0,
  CHAT_SHAPE_HAS_IMAGES = 1 << 1,
  CHAT_SHAPE_IS_OWN = 1 << 2,
  CHAT_SHAPE_SHOWS_AUTHOR = 1 << 3,
  CHAT_SHAPE_HAS_CAPTION = 1 << 4,
  CHAT_SHAPE_FAILED = 1 << 5,
};

/*
 * Slots of chatMessageSizeSpec's metrics, the fields of ChatBubbleMetrics in order.
 */
enum ChatMetricSlot {
  CHAT_METRIC_SPACING_XXS = 0,
  CHAT_METRIC_SPACING_XS,
  CHAT_METRIC_SPACING_SM,
  CHAT_METRIC_SPACING_MD,
  CHAT_METRIC_SPACING_LG,
  CHAT_METRIC_BODY_FONT_SIZE,
  CHAT_METRIC_BODY_LINE_HEIGHT,
  CHAT_METRIC_BODY_LETTER_SPACING,
  CHAT_METRIC_CAPTION_LINE_HEIGHT,
  CHAT_METRIC_SLOTS,
};

/*
 * Slots of chatMessageSizeSpec's out array.
 */
enum ChatOutSlot {
  CHAT_OUT_INSET_WIDTH = 0,
  CHAT_OUT_INSET_HEIGHT,
  CHAT_OUT_WIDTH_FRACTION,
  CHAT_OUT_SLOTS,
};

/*
 * Slots of avatarInitialsMetrics' out array, the fields of AvatarInitialsMetrics in order.
 */
enum AvatarOutSlot {
  AVATAR_OUT_FONT_SIZE = 0,
  AVATAR_OUT_LINE_HEIGHT,
  AVATAR_OUT_SLOTS,
};

/*
 * Slots of feedSingleImageFrame's out array, the fields of FeedImageFrame in order.
 */
enum FeedOutSlot {
  FEED_OUT_ASPECT_RATIO = 0,
  FEED_OUT_HEIGHT,
  FEED_OUT_SLOTS,
};

/*
 * How far relativeTime shifts the unit above the count in the long it returns.
 */
constexpr int RELATIVE_TIME_UNIT_SHIFT = 32;

void appendUtf16(std::vector<jchar>& out, char32_t codePoint) {
  if (codePoint < 0x10000) {
    out.push_back(static_cast<jchar>(codePoint));
    return;
  }
  char32_t offset = codePoint - 0x10000;
  out.push_back(static_cast<jchar>(0xD800 + (offset >> 10)));
  out.push_back(static_cast<jchar>(0xDC00 + (offset & 0x3FF)));
}

/*
 * A Java string as standard UTF-8. GetStringUTFChars gives modified UTF-8, which writes an
 * astral character as two sequences of three bytes. This reads unit by unit instead: a
 * surrogate pair is one code point, and half a pair is U+FFFD.
 */
std::string readString(JNIEnv* env, jstring value) {
  if (value == nullptr) {
    return {};
  }
  jsize length = env->GetStringLength(value);
  const jchar* units = env->GetStringChars(value, nullptr);
  if (units == nullptr) {
    return {};
  }
  std::string result;
  result.reserve(static_cast<std::size_t>(length));
  for (jsize at = 0; at < length; ++at) {
    char32_t codePoint = units[at];
    bool high = codePoint >= 0xD800 && codePoint <= 0xDBFF;
    if (high && at + 1 < length && units[at + 1] >= 0xDC00 && units[at + 1] <= 0xDFFF) {
      codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (units[at + 1] - 0xDC00);
      ++at;
    }
    sl::appendUtf8(result, codePoint);
  }
  env->ReleaseStringChars(value, units);
  return result;
}

/*
 * Standard UTF-8 as a Java string. NewStringUTF reads modified UTF-8 and breaks on the four
 * byte sequence of an astral character.
 */
jstring makeString(JNIEnv* env, const std::string& text) {
  std::vector<jchar> units;
  units.reserve(text.size());
  std::size_t offset = 0;
  while (offset < text.size()) {
    appendUtf16(units, sl::decodeUtf8(text, offset));
  }
  return env->NewString(units.data(), static_cast<jsize>(units.size()));
}

}

#define SHADOWLIST_UTILS_JNI(name) Java_com_shadowlist_utils_ShadowListUtilsCore_##name

extern "C" {

JNIEXPORT jdoubleArray JNICALL SHADOWLIST_UTILS_JNI(nativeConstants)(JNIEnv* env, jclass) {
  jdouble values[CONSTANT_SLOTS];
  values[CONSTANT_CHAT_BUBBLE_PADDING_HORIZONTAL] = sl::CHAT_BUBBLE_PADDING_HORIZONTAL;
  values[CONSTANT_CHAT_BUBBLE_WIDTH_FRACTION] = sl::CHAT_BUBBLE_WIDTH_FRACTION;
  values[CONSTANT_FEED_SINGLE_IMAGE_HEIGHT] = sl::FEED_SINGLE_IMAGE_HEIGHT;
  jdoubleArray result = env->NewDoubleArray(CONSTANT_SLOTS);
  if (result != nullptr) {
    env->SetDoubleArrayRegion(result, 0, CONSTANT_SLOTS, values);
  }
  return result;
}

JNIEXPORT jstring JNICALL SHADOWLIST_UTILS_JNI(nativeSectionOtherTitle)(JNIEnv* env, jclass) {
  return makeString(env, std::string(sl::SECTION_OTHER_TITLE));
}

JNIEXPORT jstring JNICALL SHADOWLIST_UTILS_JNI(nativeSectionInitial)(JNIEnv* env, jclass, jstring name) {
  return makeString(env, sl::sectionInitial(readString(env, name)));
}

JNIEXPORT jint JNICALL SHADOWLIST_UTILS_JNI(nativeAvatarColorIndex)(JNIEnv* env, jclass, jstring name, jint paletteSize) {
  if (paletteSize <= 0) {
    return 0;
  }
  return static_cast<jint>(sl::avatarColorIndex(readString(env, name), static_cast<std::size_t>(paletteSize)));
}

JNIEXPORT jstring JNICALL SHADOWLIST_UTILS_JNI(nativeAvatarInitials)(JNIEnv* env, jclass, jstring name) {
  return makeString(env, sl::avatarInitials(readString(env, name)));
}

JNIEXPORT void JNICALL SHADOWLIST_UTILS_JNI(nativeAvatarInitialsMetrics)(JNIEnv* env, jclass, jdouble size, jdoubleArray out) {
  if (out == nullptr || env->GetArrayLength(out) < AVATAR_OUT_SLOTS) {
    return;
  }
  sl::AvatarInitialsMetrics metrics = sl::avatarInitialsMetrics(size);
  jdouble values[AVATAR_OUT_SLOTS];
  values[AVATAR_OUT_FONT_SIZE] = metrics.fontSize;
  values[AVATAR_OUT_LINE_HEIGHT] = metrics.lineHeight;
  env->SetDoubleArrayRegion(out, 0, AVATAR_OUT_SLOTS, values);
}

JNIEXPORT jlong JNICALL SHADOWLIST_UTILS_JNI(nativeRelativeTime)(JNIEnv*, jclass, jdouble dateMs, jdouble nowMs) {
  sl::RelativeTime time = sl::relativeTime(dateMs, nowMs);
  return (static_cast<jlong>(time.unit) << RELATIVE_TIME_UNIT_SHIFT) | static_cast<jlong>(time.count);
}

JNIEXPORT jboolean JNICALL SHADOWLIST_UTILS_JNI(nativeRelativeTimeHasDate)(JNIEnv*, jclass, jdouble dateMs) {
  return sl::relativeTimeHasDate(dateMs) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL SHADOWLIST_UTILS_JNI(nativeFeedSingleImageFrame)(
  JNIEnv* env,
  jclass,
  jdouble width,
  jdouble height,
  jdoubleArray out) {
  if (out == nullptr || env->GetArrayLength(out) < FEED_OUT_SLOTS) {
    return;
  }
  sl::FeedImageFrame frame = sl::feedSingleImageFrame(width, height);
  jdouble values[FEED_OUT_SLOTS];
  values[FEED_OUT_ASPECT_RATIO] = frame.aspectRatio;
  values[FEED_OUT_HEIGHT] = frame.height;
  env->SetDoubleArrayRegion(out, 0, FEED_OUT_SLOTS, values);
}

JNIEXPORT jboolean JNICALL SHADOWLIST_UTILS_JNI(nativeChatMessageSizeSpec)(
  JNIEnv* env,
  jclass,
  jint shapeFlags,
  jdoubleArray metrics,
  jdoubleArray out) {
  if (metrics == nullptr || out == nullptr || env->GetArrayLength(metrics) < CHAT_METRIC_SLOTS ||
    env->GetArrayLength(out) < CHAT_OUT_SLOTS) {
    return JNI_FALSE;
  }
  jdouble in[CHAT_METRIC_SLOTS];
  env->GetDoubleArrayRegion(metrics, 0, CHAT_METRIC_SLOTS, in);

  sl::ChatBubbleMetrics bubble;
  bubble.spacingXxs = in[CHAT_METRIC_SPACING_XXS];
  bubble.spacingXs = in[CHAT_METRIC_SPACING_XS];
  bubble.spacingSm = in[CHAT_METRIC_SPACING_SM];
  bubble.spacingMd = in[CHAT_METRIC_SPACING_MD];
  bubble.spacingLg = in[CHAT_METRIC_SPACING_LG];
  bubble.bodyFontSize = in[CHAT_METRIC_BODY_FONT_SIZE];
  bubble.bodyLineHeight = in[CHAT_METRIC_BODY_LINE_HEIGHT];
  bubble.bodyLetterSpacing = in[CHAT_METRIC_BODY_LETTER_SPACING];
  bubble.captionLineHeight = in[CHAT_METRIC_CAPTION_LINE_HEIGHT];

  sl::ChatMessageShape shape;
  shape.hasText = (shapeFlags & CHAT_SHAPE_HAS_TEXT) != 0;
  shape.hasImages = (shapeFlags & CHAT_SHAPE_HAS_IMAGES) != 0;
  shape.isOwn = (shapeFlags & CHAT_SHAPE_IS_OWN) != 0;
  shape.showsAuthor = (shapeFlags & CHAT_SHAPE_SHOWS_AUTHOR) != 0;
  shape.hasCaption = (shapeFlags & CHAT_SHAPE_HAS_CAPTION) != 0;
  shape.failed = (shapeFlags & CHAT_SHAPE_FAILED) != 0;

  sl::RowSizeSpec spec;
  if (!sl::chatMessageSizeSpec(shape, bubble, spec)) {
    return JNI_FALSE;
  }
  jdouble values[CHAT_OUT_SLOTS];
  values[CHAT_OUT_INSET_WIDTH] = spec.insetWidth;
  values[CHAT_OUT_INSET_HEIGHT] = spec.insetHeight;
  values[CHAT_OUT_WIDTH_FRACTION] = spec.widthFraction;
  env->SetDoubleArrayRegion(out, 0, CHAT_OUT_SLOTS, values);
  return JNI_TRUE;
}

JNIEXPORT void JNICALL SHADOWLIST_UTILS_JNI(nativePollTally)(JNIEnv* env, jclass, jdoubleArray votes, jdoubleArray out) {
  enum PollOutSlot { POLL_OUT_TOTAL_VOTES, POLL_OUT_LEADING_INDEX, POLL_OUT_SLOTS };
  if (env->GetArrayLength(out) < POLL_OUT_SLOTS) {
    return;
  }
  std::vector<double> counts(static_cast<std::size_t>(env->GetArrayLength(votes)));
  if (!counts.empty()) {
    env->GetDoubleArrayRegion(votes, 0, static_cast<jsize>(counts.size()), counts.data());
  }
  sl::PollTally tally = sl::pollTally(counts);
  jdouble values[POLL_OUT_SLOTS];
  values[POLL_OUT_TOTAL_VOTES] = tally.totalVotes;
  values[POLL_OUT_LEADING_INDEX] = tally.leadingIndex == sl::UNDEFINED_INDEX ? -1.0 : static_cast<double>(tally.leadingIndex);
  env->SetDoubleArrayRegion(out, 0, POLL_OUT_SLOTS, values);
}

JNIEXPORT jint JNICALL SHADOWLIST_UTILS_JNI(nativePollPercent)(JNIEnv*, jclass, jdouble votes, jdouble totalVotes) {
  return static_cast<jint>(sl::pollPercent(votes, totalVotes));
}

JNIEXPORT void JNICALL SHADOWLIST_UTILS_JNI(nativeBoardColumnLayout)(
  JNIEnv* env, jclass, jdouble boardWidth, jdouble columnWidth, jdouble sidePadding, jdoubleArray out) {
  enum BoardLayoutSlot { BOARD_COLUMN_WIDTH = 0, BOARD_SIDE_PADDING, BOARD_SNAP_INTERVAL, BOARD_LAYOUT_SLOTS };
  auto layout = sl::boardColumnLayout(boardWidth, columnWidth, sidePadding);
  jdouble values[BOARD_LAYOUT_SLOTS];
  values[BOARD_COLUMN_WIDTH] = layout.columnWidth;
  values[BOARD_SIDE_PADDING] = layout.sidePadding;
  values[BOARD_SNAP_INTERVAL] = layout.snapInterval;
  env->SetDoubleArrayRegion(out, 0, BOARD_LAYOUT_SLOTS, values);
}

JNIEXPORT jdouble JNICALL SHADOWLIST_UTILS_JNI(nativeBoardColumnHeight)(
  JNIEnv* env, jclass, jdoubleArray heights, jdouble minimum, jdouble maximum) {
  auto count = env->GetArrayLength(heights);
  std::vector<double> values(count);
  env->GetDoubleArrayRegion(heights, 0, count, values.data());
  return sl::boardColumnHeight(values, minimum, maximum);
}


}
