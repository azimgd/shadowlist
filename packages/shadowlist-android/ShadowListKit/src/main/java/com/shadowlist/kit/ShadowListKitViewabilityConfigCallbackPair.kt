package com.shadowlist.kit

data class ShadowListKitViewabilityConfigCallbackPair(
  val viewabilityConfig: ShadowListKitViewabilityConfig,
  val onViewableItemsChanged: ((ShadowListKitViewableItemsChangedInfo) -> Unit)?,
)
