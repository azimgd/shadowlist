package com.shadowlist.utils

import android.content.Context
import android.widget.TextView

/* The native selection owner. Its immutable prepared input can be produced off the UI thread. */
internal class ShadowListUtilsAssistantDisclosureTextView(context: Context) : TextView(context) {
  var preparedLayout: ShadowListUtilsAssistantDisclosureTextLayout? = null
}
