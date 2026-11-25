package com.shadowlist.kit

data class ShadowListKitViewableItemsChangedInfo(
  val viewableItems: List<ShadowListKitViewToken>,
  val changed: List<ShadowListKitViewToken>
)
