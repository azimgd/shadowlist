package com.shadowlist.utils

import android.view.inputmethod.EditorInfo

enum class ShadowListUtilsReturnKeyType {
  DONE, NEXT, GO, SEARCH, SEND;
  internal val imeAction: Int get() = when (this) {
    DONE -> EditorInfo.IME_ACTION_DONE
    NEXT -> EditorInfo.IME_ACTION_NEXT
    GO -> EditorInfo.IME_ACTION_GO
    SEARCH -> EditorInfo.IME_ACTION_SEARCH
    SEND -> EditorInfo.IME_ACTION_SEND
  }
}
