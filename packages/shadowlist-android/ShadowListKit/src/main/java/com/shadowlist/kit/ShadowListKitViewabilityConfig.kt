package com.shadowlist.kit

/* Percentages are 0..100 and minimumViewTime is milliseconds. */
data class ShadowListKitViewabilityConfig(
  val itemVisiblePercentThreshold: Double? = null,
  val viewAreaCoveragePercentThreshold: Double? = null,
  val minimumViewTime: Double = 0.0,
  val waitForInteraction: Boolean = false,
)
