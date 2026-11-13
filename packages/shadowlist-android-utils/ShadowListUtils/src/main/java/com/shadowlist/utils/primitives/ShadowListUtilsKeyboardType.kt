package com.shadowlist.utils

import android.text.InputType

/* RN keyboard names with Android's supported native classes; iOS-only choices use text. */
enum class ShadowListUtilsKeyboardType {
  DEFAULT, ASCII_CAPABLE, NUMBERS_AND_PUNCTUATION, URL, NUMBER_PAD, PHONE_PAD, NAME_PHONE_PAD,
  EMAIL_ADDRESS, DECIMAL_PAD, TWITTER, WEB_SEARCH, ASCII_CAPABLE_NUMBER_PAD, NUMERIC, VISIBLE_PASSWORD;

  internal val inputType: Int get() = when (this) {
    NUMERIC -> InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL or InputType.TYPE_NUMBER_FLAG_SIGNED
    NUMBER_PAD -> InputType.TYPE_CLASS_NUMBER
    DECIMAL_PAD -> InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL
    PHONE_PAD -> InputType.TYPE_CLASS_PHONE
    EMAIL_ADDRESS -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS
    URL -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI
    VISIBLE_PASSWORD -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
    else -> InputType.TYPE_CLASS_TEXT
  }
}
