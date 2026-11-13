package com.shadowlist.utils

/*
 * A poll and the app's selected option. Presses never change this model automatically.
 */
data class ShadowListUtilsPollData(
  val question: String = "",
  val options: List<ShadowListUtilsPollOption> = emptyList(),
  val selectedKey: String? = null,
)
