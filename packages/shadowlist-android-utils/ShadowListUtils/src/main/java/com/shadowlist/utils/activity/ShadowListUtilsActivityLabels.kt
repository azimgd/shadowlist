package com.shadowlist.utils

/*
 * unread is spoken before an unread activity. time writes the activity's age.
 */
data class ShadowListUtilsActivityLabels(
  val unread: String = "Unread",
  val time: ShadowListUtilsRelativeTimeLabels = ShadowListUtilsRelativeTimeLabels(),
)
