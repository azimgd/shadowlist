package com.shadowlist.utils

import android.text.InputType

enum class ShadowListUtilsAutoCapitalize {
  NONE, SENTENCES, WORDS, CHARACTERS;
  internal val inputFlag: Int get() = when (this) {
    NONE -> 0
    SENTENCES -> InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
    WORDS -> InputType.TYPE_TEXT_FLAG_CAP_WORDS
    CHARACTERS -> InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
  }
}
