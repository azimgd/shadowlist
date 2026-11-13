package com.shadowlist.utils

data class ShadowListUtilsBoardLabels(
  val emptyColumn: String = "Drop cards here",
  val emptyColumnReadOnly: String = "No cards",
  val columnSummary: (String, Int) -> String = { title, count -> "$title, $count ${if (count == 1) "card" else "cards"}" },
)
