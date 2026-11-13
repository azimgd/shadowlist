package com.shadowlist.utils

import android.graphics.drawable.Drawable

/*
 * An action in the activity header. Its accessibility label defaults to the visible label.
 */
data class ShadowListUtilsActivityHeaderAction(
  val label: String,
  val onPress: () -> Unit,
  val icon: Drawable? = null,
  val accessibilityLabel: String? = null,
)
