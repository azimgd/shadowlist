package com.shadowlist.utils

/*
 * The template logic the platforms share, in the core's utils folder in C++. Every function is
 * stateless and safe on any thread. A string crosses as UTF-8, and half a surrogate pair in it
 * crosses as U+FFFD. This object is the only place of the library that calls C++.
 */
internal object ShadowListUtilsCore {
  init {
    System.loadLibrary("shadowlistutils")
  }

  /*
   * Bits of chatMessageSizeSpec's shapeFlags, the same as ChatShapeFlag in the JNI.
   */
  const val CHAT_SHAPE_HAS_TEXT = 1 shl 0
  const val CHAT_SHAPE_HAS_IMAGES = 1 shl 1
  const val CHAT_SHAPE_IS_OWN = 1 shl 2
  const val CHAT_SHAPE_SHOWS_AUTHOR = 1 shl 3
  const val CHAT_SHAPE_HAS_CAPTION = 1 shl 4
  const val CHAT_SHAPE_FAILED = 1 shl 5

  /*
   * Slots of chatMessageSizeSpec's metrics, the same as ChatMetricSlot in the JNI.
   */
  const val CHAT_METRIC_SPACING_XXS = 0
  const val CHAT_METRIC_SPACING_XS = 1
  const val CHAT_METRIC_SPACING_SM = 2
  const val CHAT_METRIC_SPACING_MD = 3
  const val CHAT_METRIC_SPACING_LG = 4
  const val CHAT_METRIC_BODY_FONT_SIZE = 5
  const val CHAT_METRIC_BODY_LINE_HEIGHT = 6
  const val CHAT_METRIC_BODY_LETTER_SPACING = 7
  const val CHAT_METRIC_CAPTION_LINE_HEIGHT = 8
  const val CHAT_METRIC_SLOTS = 9

  /*
   * Slots of chatMessageSizeSpec's out array, the same as ChatOutSlot in the JNI.
   */
  const val CHAT_OUT_INSET_WIDTH = 0
  const val CHAT_OUT_INSET_HEIGHT = 1
  const val CHAT_OUT_WIDTH_FRACTION = 2
  const val CHAT_OUT_SLOTS = 3

  /*
   * Slots of avatarInitialsMetrics' out array, the same as AvatarOutSlot in the JNI.
   */
  const val AVATAR_OUT_FONT_SIZE = 0
  const val AVATAR_OUT_LINE_HEIGHT = 1
  const val AVATAR_OUT_SLOTS = 2

  /*
   * Slots of feedSingleImageFrame's out array, the same as FeedOutSlot in the JNI.
   */
  const val FEED_OUT_ASPECT_RATIO = 0
  const val FEED_OUT_HEIGHT = 1
  const val FEED_OUT_SLOTS = 2

  /*
   * Units of relativeTime, the same order as the core's RelativeTimeUnit.
   */
  const val RELATIVE_TIME_NOW = 0
  const val RELATIVE_TIME_MINUTES = 1
  const val RELATIVE_TIME_HOURS = 2
  const val RELATIVE_TIME_DAYS = 3
  const val RELATIVE_TIME_DATE = 4

  /*
   * How far relativeTime shifts the unit above the count, the same as in the JNI.
   */
  const val RELATIVE_TIME_UNIT_SHIFT = 32

  /*
   * Slots of constants, the same as ConstantSlot in the JNI.
   */
  private const val CONSTANT_CHAT_BUBBLE_PADDING_HORIZONTAL = 0
  private const val CONSTANT_CHAT_BUBBLE_WIDTH_FRACTION = 1
  private const val CONSTANT_FEED_SINGLE_IMAGE_HEIGHT = 2

  /*
   * Slots of pollTally's output, the same as PollOutSlot in the JNI.
   */
  const val POLL_OUT_TOTAL_VOTES = 0
  const val POLL_OUT_LEADING_INDEX = 1
  const val POLL_OUT_SLOTS = 2

  private val constants: DoubleArray by lazy { nativeConstants() }

  /*
   * The core's chat constants: the padding at each side of a bubble's text, and the share of
   * the row's width a bubble's column takes.
   */
  val CHAT_BUBBLE_PADDING_HORIZONTAL_DP: Float get() = constants[CONSTANT_CHAT_BUBBLE_PADDING_HORIZONTAL].toFloat()
  val CHAT_BUBBLE_WIDTH_FRACTION: Float get() = constants[CONSTANT_CHAT_BUBBLE_WIDTH_FRACTION].toFloat()

  /*
   * The core's feed constant: the height of an image without a size, and of every image of a strip.
   */
  val FEED_SINGLE_IMAGE_HEIGHT_DP: Float get() = constants[CONSTANT_FEED_SINGLE_IMAGE_HEIGHT].toFloat()

  /*
   * The title of the section for names that start with no letter.
   */
  val SECTION_OTHER_TITLE: String by lazy { nativeSectionOtherTitle() }

  /*
   * The palette slot of a name, the same on every platform.
   */
  fun avatarColorIndex(name: String, paletteSize: Int): Int = nativeAvatarColorIndex(name, paletteSize)

  /*
   * The first character of the name's first word and of its last word when it has more than
   * one. The case is the name's.
   */
  fun avatarInitials(name: String): String = nativeAvatarInitials(name)

  /*
   * The font size and the line box of the initials in an avatar of a size into out, in dp. The
   * initials keep that size whatever the reader's text size setting.
   */
  fun avatarInitialsMetrics(size: Double, out: DoubleArray) = nativeAvatarInitialsMetrics(size, out)

  /*
   * How long ago dateMs was at nowMs: the unit in the high 32 bits and the count in the low.
   * ShadowListUtilsRelativeTime unpacks it.
   */
  fun relativeTime(dateMs: Double, nowMs: Double): Long = nativeRelativeTime(dateMs, nowMs)

  /*
   * Whether dateMs is a moment that can be written as a calendar date.
   */
  fun relativeTimeHasDate(dateMs: Double): Boolean = nativeRelativeTimeHasDate(dateMs)

  /*
   * The frame of a feed post's single image into out: an aspect ratio, or a fixed height in dp.
   * The one that does not apply is 0.
   */
  fun feedSingleImageFrame(width: Double, height: Double, out: DoubleArray) =
    nativeFeedSingleImageFrame(width, height, out)

  /*
   * The insets and the width fraction of a text message's bubble into out, in dp. Returns false
   * for a message the row lays out itself: one with images or without text.
   */
  fun chatMessageSizeSpec(shapeFlags: Int, metrics: DoubleArray, out: DoubleArray): Boolean =
    nativeChatMessageSizeSpec(shapeFlags, metrics, out)

  /*
   * The title of the section a contact's name falls in: an upper case ASCII letter, the name's
   * first code point as written when it is outside ASCII, or SECTION_OTHER_TITLE.
   */
  fun sectionInitial(name: String): String = nativeSectionInitial(name)

  fun pollTally(votes: DoubleArray, out: DoubleArray) = nativePollTally(votes, out)
  fun pollPercent(votes: Double, totalVotes: Double): Int = nativePollPercent(votes, totalVotes)

  @JvmStatic private external fun nativePollTally(votes: DoubleArray, out: DoubleArray)
  @JvmStatic private external fun nativePollPercent(votes: Double, totalVotes: Double): Int
  @JvmStatic private external fun nativeConstants(): DoubleArray
  @JvmStatic private external fun nativeSectionOtherTitle(): String
  @JvmStatic private external fun nativeSectionInitial(name: String): String
  @JvmStatic private external fun nativeAvatarColorIndex(name: String, paletteSize: Int): Int
  @JvmStatic private external fun nativeAvatarInitials(name: String): String
  @JvmStatic private external fun nativeAvatarInitialsMetrics(size: Double, out: DoubleArray)
  @JvmStatic private external fun nativeRelativeTime(dateMs: Double, nowMs: Double): Long
  @JvmStatic private external fun nativeRelativeTimeHasDate(dateMs: Double): Boolean
  @JvmStatic private external fun nativeFeedSingleImageFrame(width: Double, height: Double, out: DoubleArray)
  @JvmStatic private external fun nativeChatMessageSizeSpec(shapeFlags: Int, metrics: DoubleArray, out: DoubleArray): Boolean
}

/*
 * What relativeTime packs into a long.
 */
@JvmInline
internal value class ShadowListUtilsRelativeTime(private val packed: Long) {
  val unit: Int get() = (packed ushr ShadowListUtilsCore.RELATIVE_TIME_UNIT_SHIFT).toInt()
  val count: Int get() = (packed and 0xffffffffL).toInt()
}
