package com.shadowlist.utils

import java.text.NumberFormat

/*
 * totalVotes writes the footer. option is the label a screen reader says with its whole percent.
 */
data class ShadowListUtilsPollLabels(
  val totalVotes: (Double) -> String = { count ->
    if (count == 1.0) "1 vote" else "${NumberFormat.getNumberInstance().format(count)} votes"
  },
  val option: (String, Int) -> String = { label, percent -> "$label, $percent%" },
)
