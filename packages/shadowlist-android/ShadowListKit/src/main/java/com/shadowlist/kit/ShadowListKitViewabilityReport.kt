package com.shadowlist.kit

import androidx.annotation.Keep

@Keep
internal class ShadowListKitViewabilityReport(
  val config: Int,
  val viewable: Array<String>,
  val added: Array<String>,
  val removed: Array<String>,
)
